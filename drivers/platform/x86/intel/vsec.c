// SPDX-License-Identifier: GPL-2.0
/*
 * Intel Vendor Specific Extended Capabilities auxiliary bus driver
 *
 * Copyright (c) 2021, Intel Corporation.
 * All Rights Reserved.
 *
 * Author: David E. Box <david.e.box@linux.intel.com>
 *
 * This driver discovers and creates auxiliary devices for Intel defined PCIe
 * "Vendor Specific" and "Designated Vendor Specific" Extended Capabilities,
 * VSEC and DVSEC respectively. The driver supports features on specific PCIe
 * endpoints that exist primarily to expose them.
 */

#include <linux/auxiliary_bus.h>
#include <linux/bits.h>
#include <linux/kernel.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/types.h>

#ifndef _UNCONTAINED_KCALLOC_H
#define _UNCONTAINED_KCALLOC_H
static volatile unsigned long __uncontained_kcalloc;
#endif /*_UNCONTAINED_KCALLOC_H*/

#include "vsec.h"

/* Intel DVSEC offsets */
#define INTEL_DVSEC_ENTRIES		0xA
#define INTEL_DVSEC_SIZE		0xB
#define INTEL_DVSEC_TABLE		0xC
#define INTEL_DVSEC_TABLE_BAR(x)	((x) & GENMASK(2, 0))
#define INTEL_DVSEC_TABLE_OFFSET(x)	((x) & GENMASK(31, 3))
#define TABLE_OFFSET_SHIFT		3

static DEFINE_IDA(intel_vsec_ida);

/**
 * struct intel_vsec_header - Common fields of Intel VSEC and DVSEC registers.
 * @rev:         Revision ID of the VSEC/DVSEC register space
 * @length:      Length of the VSEC/DVSEC register space
 * @id:          ID of the feature
 * @num_entries: Number of instances of the feature
 * @entry_size:  Size of the discovery table for each feature
 * @tbir:        BAR containing the discovery tables
 * @offset:      BAR offset of start of the first discovery table
 */
struct intel_vsec_header {
	u8	rev;
	u16	length;
	u16	id;
	u8	num_entries;
	u8	entry_size;
	u8	tbir;
	u32	offset;
};

/* Platform specific data */
struct intel_vsec_platform_info {
	struct intel_vsec_header **capabilities;
	unsigned long quirks;
};

enum intel_vsec_id {
	VSEC_ID_TELEMETRY	= 2,
	VSEC_ID_WATCHER		= 3,
	VSEC_ID_CRASHLOG	= 4,
};

static enum intel_vsec_id intel_vsec_allow_list[] = {
	VSEC_ID_TELEMETRY,
	VSEC_ID_WATCHER,
	VSEC_ID_CRASHLOG,
};

static const char *intel_vsec_name(enum intel_vsec_id id)
{
	switch (id) {
	case VSEC_ID_TELEMETRY:
		return "telemetry";

	case VSEC_ID_WATCHER:
		return "watcher";

	case VSEC_ID_CRASHLOG:
		return "crashlog";

	default:
		return NULL;
	}
}

static bool intel_vsec_allowed(u16 id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(intel_vsec_allow_list); i++)
		if (intel_vsec_allow_list[i] == id)
			return true;

	return false;
}

static bool intel_vsec_disabled(u16 id, unsigned long quirks)
{
	switch (id) {
	case VSEC_ID_WATCHER:
		return !!(quirks & VSEC_QUIRK_NO_WATCHER);

	case VSEC_ID_CRASHLOG:
		return !!(quirks & VSEC_QUIRK_NO_CRASHLOG);

	default:
		return false;
	}
}

static void intel_vsec_remove_aux(void *data)
{
	auxiliary_device_delete(data);
	auxiliary_device_uninit(data);
}

static void intel_vsec_dev_release(struct device *dev)
{
	struct intel_vsec_device *intel_vsec_dev = dev_to_ivdev(dev);

	ida_free(intel_vsec_dev->ida, intel_vsec_dev->auxdev.id);
	kfree(intel_vsec_dev->resource);
	kfree(intel_vsec_dev);
}

static int intel_vsec_add_aux(struct pci_dev *pdev, struct intel_vsec_device *intel_vsec_dev,
			      const char *name)
{
	struct auxiliary_device *auxdev = &intel_vsec_dev->auxdev;
	int ret;

	ret = ida_alloc(intel_vsec_dev->ida, GFP_KERNEL);
	if (ret < 0) {
		kfree(intel_vsec_dev);
		return ret;
	}

	auxdev->id = ret;
	auxdev->name = name;
	auxdev->dev.parent = &pdev->dev;
	auxdev->dev.release = intel_vsec_dev_release;

	ret = auxiliary_device_init(auxdev);
	if (ret < 0) {
		ida_free(intel_vsec_dev->ida, auxdev->id);
		kfree(intel_vsec_dev->resource);
		kfree(intel_vsec_dev);
		return ret;
	}

	ret = auxiliary_device_add(auxdev);
	if (ret < 0) {
		auxiliary_device_uninit(auxdev);
		return ret;
	}

	return devm_add_action_or_reset(&pdev->dev, intel_vsec_remove_aux, auxdev);
}

static int intel_vsec_add_dev(struct pci_dev *pdev, struct intel_vsec_header *header,
			   unsigned long quirks)
{
	struct intel_vsec_device *intel_vsec_dev;
	struct resource *res, *tmp;
	int i;

	if (!intel_vsec_allowed(header->id) || intel_vsec_disabled(header->id, quirks))
		return -EINVAL;

	if (!header->num_entries) {
		dev_dbg(&pdev->dev, "Invalid 0 entry count for header id %d\n", header->id);
		return -EINVAL;
	}

	if (!header->entry_size) {
		dev_dbg(&pdev->dev, "Invalid 0 entry size for header id %d\n", header->id);
		return -EINVAL;
	}

	intel_vsec_dev = kzalloc(sizeof(*intel_vsec_dev), GFP_KERNEL);
	if (!intel_vsec_dev)
		return -ENOMEM;

	res = kcalloc(header->num_entries, sizeof(*res), GFP_KERNEL);
	{
		typeof((*res)) __uncontained_tmp240;
		__uncontained_kcalloc = (unsigned long)&__uncontained_tmp240;
	}
	if (!res) {
		kfree(intel_vsec_dev);
		return -ENOMEM;
	}

	if (quirks & VSEC_QUIRK_TABLE_SHIFT)
		header->offset >>= TABLE_OFFSET_SHIFT;

	/*
	 * The DVSEC/VSEC contains the starting offset and count for a block of
	 * discovery tables. Create a resource array of these tables to the
	 * auxiliary device driver.
	 */
	for (i = 0, tmp = res; i < header->num_entries; i++, tmp++) {
		tmp->start = pdev->resource[header->tbir].start +
			     header->offset + i * (header->entry_size * sizeof(u32));
		tmp->end = tmp->start + (header->entry_size * sizeof(u32)) - 1;
		tmp->flags = IORESOURCE_MEM;
	}

	intel_vsec_dev->pcidev = pdev;
	intel_vsec_dev->resource = res;
	intel_vsec_dev->num_resources = header->num_entries;
	intel_vsec_dev->quirks = quirks;
	intel_vsec_dev->ida = &intel_vsec_ida;

	return intel_vsec_add_aux(pdev, intel_vsec_dev, intel_vsec_name(header->id));
}

static bool intel_vsec_walk_header(struct pci_dev *pdev, unsigned long quirks,
				struct intel_vsec_header **header)
{
	bool have_devices = false;
	int ret;

	for ( ; *header; header++) {
		ret = intel_vsec_add_dev(pdev, *header, quirks);
		if (ret)
			dev_info(&pdev->dev, "Could not add device for DVSEC id %d\n",
				 (*header)->id);
		else
			have_devices = true;
	}

	return have_devices;
}

static bool intel_vsec_walk_dvsec(struct pci_dev *pdev, unsigned long quirks)
{
	bool have_devices = false;
	int pos = 0;

	do {
		struct intel_vsec_header header;
		u32 table, hdr;
		u16 vid;
		int ret;

		pos = pci_find_next_ext_capability(pdev, pos, PCI_EXT_CAP_ID_DVSEC);
		if (!pos)
			break;

		pci_read_config_dword(pdev, pos + PCI_DVSEC_HEADER1, &hdr);
		vid = PCI_DVSEC_HEADER1_VID(hdr);
		if (vid != PCI_VENDOR_ID_INTEL)
			continue;

		/* Support only revision 1 */
		header.rev = PCI_DVSEC_HEADER1_REV(hdr);
		if (header.rev != 1) {
			dev_info(&pdev->dev, "Unsupported DVSEC revision %d\n", header.rev);
			continue;
		}

		header.length = PCI_DVSEC_HEADER1_LEN(hdr);

		pci_read_config_byte(pdev, pos + INTEL_DVSEC_ENTRIES, &header.num_entries);
		pci_read_config_byte(pdev, pos + INTEL_DVSEC_SIZE, &header.entry_size);
		pci_read_config_dword(pdev, pos + INTEL_DVSEC_TABLE, &table);

		header.tbir = INTEL_DVSEC_TABLE_BAR(table);
		header.offset = INTEL_DVSEC_TABLE_OFFSET(table);

		pci_read_config_dword(pdev, pos + PCI_DVSEC_HEADER2, &hdr);
		header.id = PCI_DVSEC_HEADER2_ID(hdr);

		ret = intel_vsec_add_dev(pdev, &header, quirks);
		if (ret)
			continue;

		have_devices = true;
	} while (true);

	return have_devices;
}

static bool intel_vsec_walk_vsec(struct pci_dev *pdev, unsigned long quirks)
{
	bool have_devices = false;
	int pos = 0;

	do {
		struct intel_vsec_header header;
		u32 table, hdr;
		int ret;

		pos = pci_find_next_ext_capability(pdev, pos, PCI_EXT_CAP_ID_VNDR);
		if (!pos)
			break;

		pci_read_config_dword(pdev, pos + PCI_VNDR_HEADER, &hdr);

		/* Support only revision 1 */
		header.rev = PCI_VNDR_HEADER_REV(hdr);
		if (header.rev != 1) {
			dev_info(&pdev->dev, "Unsupported VSEC revision %d\n", header.rev);
			continue;
		}

		header.id = PCI_VNDR_HEADER_ID(hdr);
		header.length = PCI_VNDR_HEADER_LEN(hdr);

		/* entry, size, and table offset are the same as DVSEC */
		pci_read_config_byte(pdev, pos + INTEL_DVSEC_ENTRIES, &header.num_entries);
		pci_read_config_byte(pdev, pos + INTEL_DVSEC_SIZE, &header.entry_size);
		pci_read_config_dword(pdev, pos + INTEL_DVSEC_TABLE, &table);

		header.tbir = INTEL_DVSEC_TABLE_BAR(table);
		header.offset = INTEL_DVSEC_TABLE_OFFSET(table);

		ret = intel_vsec_add_dev(pdev, &header, quirks);
		if (ret)
			continue;

		have_devices = true;
	} while (true);

	return have_devices;
}

static int intel_vsec_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct intel_vsec_platform_info *info;
	bool have_devices = false;
	unsigned long quirks = 0;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	info = (struct intel_vsec_platform_info *)id->driver_data;
	if (info)
		quirks = info->quirks;

	if (intel_vsec_walk_dvsec(pdev, quirks))
		have_devices = true;

	if (intel_vsec_walk_vsec(pdev, quirks))
		have_devices = true;

	if (info && (info->quirks & VSEC_QUIRK_NO_DVSEC) &&
	    intel_vsec_walk_header(pdev, quirks, info->capabilities))
		have_devices = true;

	if (!have_devices)
		return -ENODEV;

	return 0;
}

/* TGL info */
static const struct intel_vsec_platform_info tgl_info = {
	.quirks = VSEC_QUIRK_NO_WATCHER | VSEC_QUIRK_NO_CRASHLOG | VSEC_QUIRK_TABLE_SHIFT,
};

/* DG1 info */
static struct intel_vsec_header dg1_telemetry = {
	.length = 0x10,
	.id = 2,
	.num_entries = 1,
	.entry_size = 3,
	.tbir = 0,
	.offset = 0x466000,
};

static struct intel_vsec_header *dg1_capabilities[] = {
	&dg1_telemetry,
	NULL
};

static const struct intel_vsec_platform_info dg1_info = {
	.capabilities = dg1_capabilities,
	.quirks = VSEC_QUIRK_NO_DVSEC,
};

#define PCI_DEVICE_ID_INTEL_VSEC_ADL		0x467d
#define PCI_DEVICE_ID_INTEL_VSEC_DG1		0x490e
#define PCI_DEVICE_ID_INTEL_VSEC_OOBMSM		0x09a7
#define PCI_DEVICE_ID_INTEL_VSEC_TGL		0x9a0d
static const struct pci_device_id intel_vsec_pci_ids[] = {
	{ PCI_DEVICE_DATA(INTEL, VSEC_ADL, &tgl_info) },
	{ PCI_DEVICE_DATA(INTEL, VSEC_DG1, &dg1_info) },
	{ PCI_DEVICE_DATA(INTEL, VSEC_OOBMSM, NULL) },
	{ PCI_DEVICE_DATA(INTEL, VSEC_TGL, &tgl_info) },
	{ }
};
MODULE_DEVICE_TABLE(pci, intel_vsec_pci_ids);

static struct pci_driver intel_vsec_pci_driver = {
	.name = "intel_vsec",
	.id_table = intel_vsec_pci_ids,
	.probe = intel_vsec_pci_probe,
};
module_pci_driver(intel_vsec_pci_driver);

MODULE_AUTHOR("David E. Box <david.e.box@linux.intel.com>");
MODULE_DESCRIPTION("Intel Extended Capabilities auxiliary bus driver");
MODULE_LICENSE("GPL v2");
