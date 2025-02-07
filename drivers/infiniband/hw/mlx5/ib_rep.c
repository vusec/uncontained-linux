// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
/*
 * Copyright (c) 2018 Mellanox Technologies. All rights reserved.
 */

#include <linux/mlx5/vport.h>

#ifndef _UNCONTAINED_KCALLOC_H
#define _UNCONTAINED_KCALLOC_H
static volatile unsigned long __uncontained_kcalloc;
#endif /*_UNCONTAINED_KCALLOC_H*/
#include "ib_rep.h"
#include "srq.h"

static int
mlx5_ib_set_vport_rep(struct mlx5_core_dev *dev,
		      struct mlx5_eswitch_rep *rep,
		      int vport_index)
{
	struct mlx5_ib_dev *ibdev;

	ibdev = mlx5_eswitch_uplink_get_proto_dev(dev->priv.eswitch, REP_IB);
	if (!ibdev)
		return -EINVAL;

	ibdev->port[vport_index].rep = rep;
	rep->rep_data[REP_IB].priv = ibdev;
	write_lock(&ibdev->port[vport_index].roce.netdev_lock);
	ibdev->port[vport_index].roce.netdev =
		mlx5_ib_get_rep_netdev(rep->esw, rep->vport);
	write_unlock(&ibdev->port[vport_index].roce.netdev_lock);

	return 0;
}

static void mlx5_ib_register_peer_vport_reps(struct mlx5_core_dev *mdev);

static int
mlx5_ib_vport_rep_load(struct mlx5_core_dev *dev, struct mlx5_eswitch_rep *rep)
{
	u32 num_ports = mlx5_eswitch_get_total_vports(dev);
	const struct mlx5_ib_profile *profile;
	struct mlx5_core_dev *peer_dev;
	struct mlx5_ib_dev *ibdev;
	u32 peer_num_ports;
	int vport_index;
	int ret;

	vport_index = rep->vport_index;

	if (mlx5_lag_is_shared_fdb(dev)) {
		peer_dev = mlx5_lag_get_peer_mdev(dev);
		peer_num_ports = mlx5_eswitch_get_total_vports(peer_dev);
		if (mlx5_lag_is_master(dev)) {
			/* Only 1 ib port is the representor for both uplinks */
			num_ports += peer_num_ports - 1;
		} else {
			if (rep->vport == MLX5_VPORT_UPLINK)
				return 0;
			vport_index += peer_num_ports;
			dev = peer_dev;
		}
	}

	if (rep->vport == MLX5_VPORT_UPLINK)
		profile = &raw_eth_profile;
	else
		return mlx5_ib_set_vport_rep(dev, rep, vport_index);

	ibdev = ib_alloc_device(mlx5_ib_dev, ib_dev);
	if (!ibdev)
		return -ENOMEM;

	ibdev->port = kcalloc(num_ports, sizeof(*ibdev->port),
			      GFP_KERNEL);
	{
		typeof((*ibdev->port)) __uncontained_tmp111;
		__uncontained_kcalloc = (unsigned long)&__uncontained_tmp111;
	}
	if (!ibdev->port) {
		ret = -ENOMEM;
		goto fail_port;
	}

	ibdev->is_rep = true;
	vport_index = rep->vport_index;
	ibdev->port[vport_index].rep = rep;
	ibdev->port[vport_index].roce.netdev =
		mlx5_ib_get_rep_netdev(dev->priv.eswitch, rep->vport);
	ibdev->mdev = dev;
	ibdev->num_ports = num_ports;

	ret = __mlx5_ib_add(ibdev, profile);
	if (ret)
		goto fail_add;

	rep->rep_data[REP_IB].priv = ibdev;
	if (mlx5_lag_is_shared_fdb(dev))
		mlx5_ib_register_peer_vport_reps(dev);

	return 0;

fail_add:
	kfree(ibdev->port);
fail_port:
	ib_dealloc_device(&ibdev->ib_dev);
	return ret;
}

static void *mlx5_ib_rep_to_dev(struct mlx5_eswitch_rep *rep)
{
	return rep->rep_data[REP_IB].priv;
}

static void
mlx5_ib_vport_rep_unload(struct mlx5_eswitch_rep *rep)
{
	struct mlx5_core_dev *mdev = mlx5_eswitch_get_core_dev(rep->esw);
	struct mlx5_ib_dev *dev = mlx5_ib_rep_to_dev(rep);
	int vport_index = rep->vport_index;
	struct mlx5_ib_port *port;

	if (WARN_ON(!mdev))
		return;

	if (mlx5_lag_is_shared_fdb(mdev) &&
	    !mlx5_lag_is_master(mdev)) {
		struct mlx5_core_dev *peer_mdev;

		if (rep->vport == MLX5_VPORT_UPLINK)
			return;
		peer_mdev = mlx5_lag_get_peer_mdev(mdev);
		vport_index += mlx5_eswitch_get_total_vports(peer_mdev);
	}

	if (!dev)
		return;

	port = &dev->port[vport_index];
	write_lock(&port->roce.netdev_lock);
	port->roce.netdev = NULL;
	write_unlock(&port->roce.netdev_lock);
	rep->rep_data[REP_IB].priv = NULL;
	port->rep = NULL;

	if (rep->vport == MLX5_VPORT_UPLINK) {
		struct mlx5_core_dev *peer_mdev;
		struct mlx5_eswitch *esw;

		if (mlx5_lag_is_shared_fdb(mdev)) {
			peer_mdev = mlx5_lag_get_peer_mdev(mdev);
			esw = peer_mdev->priv.eswitch;
			mlx5_eswitch_unregister_vport_reps(esw, REP_IB);
		}
		__mlx5_ib_remove(dev, dev->profile, MLX5_IB_STAGE_MAX);
	}
}

static const struct mlx5_eswitch_rep_ops rep_ops = {
	.load = mlx5_ib_vport_rep_load,
	.unload = mlx5_ib_vport_rep_unload,
	.get_proto_dev = mlx5_ib_rep_to_dev,
};

static void mlx5_ib_register_peer_vport_reps(struct mlx5_core_dev *mdev)
{
	struct mlx5_core_dev *peer_mdev = mlx5_lag_get_peer_mdev(mdev);
	struct mlx5_eswitch *esw;

	if (!peer_mdev)
		return;

	esw = peer_mdev->priv.eswitch;
	mlx5_eswitch_register_vport_reps(esw, &rep_ops, REP_IB);
}

struct net_device *mlx5_ib_get_rep_netdev(struct mlx5_eswitch *esw,
					  u16 vport_num)
{
	return mlx5_eswitch_get_proto_dev(esw, vport_num, REP_ETH);
}

struct mlx5_flow_handle *create_flow_rule_vport_sq(struct mlx5_ib_dev *dev,
						   struct mlx5_ib_sq *sq,
						   u32 port)
{
	struct mlx5_eswitch *esw = dev->mdev->priv.eswitch;
	struct mlx5_eswitch_rep *rep;

	if (!dev->is_rep || !port)
		return NULL;

	if (!dev->port[port - 1].rep)
		return ERR_PTR(-EINVAL);

	rep = dev->port[port - 1].rep;

	return mlx5_eswitch_add_send_to_vport_rule(esw, esw, rep, sq->base.mqp.qpn);
}

static int mlx5r_rep_probe(struct auxiliary_device *adev,
			   const struct auxiliary_device_id *id)
{
	struct mlx5_adev *idev = container_of(adev, struct mlx5_adev, adev);
	struct mlx5_core_dev *mdev = idev->mdev;
	struct mlx5_eswitch *esw;

	esw = mdev->priv.eswitch;
	mlx5_eswitch_register_vport_reps(esw, &rep_ops, REP_IB);
	return 0;
}

static void mlx5r_rep_remove(struct auxiliary_device *adev)
{
	struct mlx5_adev *idev = container_of(adev, struct mlx5_adev, adev);
	struct mlx5_core_dev *mdev = idev->mdev;
	struct mlx5_eswitch *esw;

	esw = mdev->priv.eswitch;
	mlx5_eswitch_unregister_vport_reps(esw, REP_IB);
}

static const struct auxiliary_device_id mlx5r_rep_id_table[] = {
	{ .name = MLX5_ADEV_NAME ".rdma-rep", },
	{},
};

MODULE_DEVICE_TABLE(auxiliary, mlx5r_rep_id_table);

static struct auxiliary_driver mlx5r_rep_driver = {
	.name = "rep",
	.probe = mlx5r_rep_probe,
	.remove = mlx5r_rep_remove,
	.id_table = mlx5r_rep_id_table,
};

int mlx5r_rep_init(void)
{
	return auxiliary_driver_register(&mlx5r_rep_driver);
}

void mlx5r_rep_cleanup(void)
{
	auxiliary_driver_unregister(&mlx5r_rep_driver);
}
