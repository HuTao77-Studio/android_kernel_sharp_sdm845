KASAN_SANITIZE_scm.o := n
KCOV_INSTRUMENT_scm.o := n

obj-$(CONFIG_QCOM_CPUSS_DUMP) += cpuss_dump.o
obj-$(CONFIG_CPU_V7) += idle-v7.o
obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_QCOM_LLCC) += llcc-core.o llcc-slice.o
obj-$(CONFIG_QCOM_SDM845_LLCC) += llcc-sdm845.o
obj-$(CONFIG_QCOM_SDM670_LLCC) += llcc-sdm670.o
obj-$(CONFIG_QCOM_QCS605_LLCC) += llcc-qcs605.o
obj-$(CONFIG_QCOM_LLCC_PERFMON) += llcc_perfmon.o
obj-$(CONFIG_QCOM_LLCC_AMON) += llcc-amon.o
obj-$(CONFIG_QPNP_PBS) += qpnp-pbs.o
obj-$(CONFIG_QCOM_PM)	+=	spm.o
obj-$(CONFIG_QCOM_SMD) +=	smd.o
obj-$(CONFIG_QCOM_SMD_RPM)	+= smd-rpm.o
obj-$(CONFIG_QCOM_SMEM) +=	smem.o
obj-$(CONFIG_MSM_PM_LEGACY) += pm-boot.o msm-pm.o
obj-$(CONFIG_MSM_SPM) += msm-spm.o spm_devices.o
obj-$(CONFIG_QCOM_SMEM_STATE) += smem_state.o
obj-$(CONFIG_QCOM_SMP2P)	+= smp2p.o
obj-$(CONFIG_QCOM_SMSM)	+= smsm.o
obj-$(CONFIG_QCOM_WCNSS_CTRL) += wcnss_ctrl.o
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)
obj-$(CONFIG_QCOM_SCM)  +=      scm.o scm-boot.o
obj-$(CONFIG_QCOM_EARLY_RANDOM)	+= early_random.o
obj-$(CONFIG_SOC_BUS) += socinfo.o
obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o
obj-$(CONFIG_MSM_BOOT_TIME_MARKER) += boot_marker.o
obj-$(CONFIG_MSM_CORE_HANG_DETECT) += core_hang_detect.o
obj-$(CONFIG_MSM_GLADIATOR_HANG_DETECT) += gladiator_hang_detect.o
obj-$(CONFIG_MSM_GLADIATOR_ERP) += gladiator_erp.o
obj-$(CONFIG_QCOM_EUD) += eud.o
obj-$(CONFIG_QCOM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_QCOM_MEMORY_DUMP_V2) += memory_dump_v2.o
obj-$(CONFIG_QCOM_MINIDUMP) += msm_minidump.o minidump_log.o
obj-$(CONFIG_QCOM_RUN_QUEUE_STATS) += rq_stats.o
obj-$(CONFIG_QCOM_SECURE_BUFFER) += secure_buffer.o
obj-$(CONFIG_MSM_SMEM) += msm_smem.o smem_debug.o
obj-$(CONFIG_MSM_SMD)	+= msm_smd.o smd_debug.o smd_private.o smd_init_dt.o smsm_debug.o
obj-$(CONFIG_SHARP_SMEM_CUST)	+=	sh_smem.o
obj-$(CONFIG_MSM_GLINK) += glink.o glink_debugfs.o glink_ssr.o
obj-$(CONFIG_MSM_TZ_SMMU) += msm_tz_smmu.o
obj-$(CONFIG_MSM_GLINK_LOOPBACK_SERVER) += glink_loopback_server.o
obj-$(CONFIG_MSM_GLINK_BGCOM_XPRT) += glink_bgcom_xprt.o
obj-$(CONFIG_MSM_GLINK_SMEM_NATIVE_XPRT) += glink_smem_native_xprt.o
obj-$(CONFIG_MSM_GLINK_SPI_XPRT) += glink_spi_xprt.o
obj-$(CONFIG_MSM_SPCOM) += spcom.o
obj-$(CONFIG_MSM_SPSS_UTILS) += spss_utils.o
obj-$(CONFIG_TRACER_PKT) += tracer_pkt.o
obj-$(CONFIG_QCOM_BUS_SCALING) += msm_bus/
obj-$(CONFIG_QTI_RPMH_API) += rpmh.o
obj-$(CONFIG_MSM_RPM_SMD)   +=  rpm-smd.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_RPM_SMD)   +=  rpm-smd-debug.o
endif
obj-$(CONFIG_QTI_SYSTEM_PM) += system_pm.o
obj-$(CONFIG_MSM_SERVICE_NOTIFIER) += service-notifier.o
obj-$(CONFIG_MSM_SERVICE_LOCATOR) += service-locator.o
obj-$(CONFIG_MSM_SMP2P) += msm_smp2p.o smp2p_loopback.o smp2p_debug.o smp2p_sleepstate.o
obj-$(CONFIG_MSM_IPC_ROUTER_SMD_XPRT) += ipc_router_smd_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_USB_XPRT) += ipc_router_usb_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_MHI_XPRT) += ipc_router_mhi_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_MHI_DEV_XPRT) += ipc_router_mhi_dev_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_GLINK_XPRT) += ipc_router_glink_xprt.o
obj-$(CONFIG_MSM_QMI_INTERFACE) += qmi_interface.o
obj-$(CONFIG_MSM_GLINK_PKT) += msm_glink_pkt.o
obj-y			+=	qdsp6v2/
obj-$(CONFIG_MSM_SYSTEM_HEALTH_MONITOR)	+=	system_health_monitor_v01.o
obj-$(CONFIG_MSM_SYSTEM_HEALTH_MONITOR)	+=	system_health_monitor.o
obj-$(CONFIG_MSM_SYSMON_GLINK_COMM) += sysmon-glink.o sysmon-qmi.o
obj-$(CONFIG_ICNSS) += icnss.o wlan_firmware_service_v01.o

obj-$(CONFIG_MEM_SHARE_QMI_SERVICE)		+= memshare/
obj-$(CONFIG_MSM_PIL)   +=      peripheral-loader.o
obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_MSM_PIL_SSR_BG) += subsys-pil-bg.o
obj-$(CONFIG_MSM_PIL_MSS_QDSP6V5) += pil-q6v5.o pil-msa.o pil-q6v5-mss.o
obj-$(CONFIG_MSM_BGCOM) += bgcom_spi.o

obj-$(CONFIG_MSM_PERFORMANCE) += msm_performance.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
       obj-y += subsystem_notif.o
       obj-y += subsystem_restart.o
       obj-y += ramdump.o
       obj-y += microdump_collector.o
endif
obj-$(CONFIG_MSM_SYSMON_COMM) += sysmon.o sysmon-qmi.o
obj-$(CONFIG_MSM_BGRSB) += bg_rsb.o
obj-$(CONFIG_MSM_BGCOM_INTERFACE) += bgcom_interface.o
obj-$(CONFIG_MSM_JTAGV8) += jtagv8.o jtagv8-etm.o
obj-$(CONFIG_QCOM_COMMAND_DB) += cmd-db.o
obj-$(CONFIG_MSM_QBT1000) += qbt1000.o
obj-$(CONFIG_MSM_EVENT_TIMER) += event_timer.o
obj-$(CONFIG_MSM_IDLE_STATS)	+= lpm-stats.o
obj-$(CONFIG_QCOM_DCC) += dcc.o
obj-$(CONFIG_QCOM_DCC_V2) += dcc_v2.o
obj-$(CONFIG_QTI_RPM_STATS_LOG) += rpm_stats.o
ifdef CONFIG_MSM_RPM_SMD
	obj-$(CONFIG_QTI_RPM_STATS_LOG) += rpm_master_stat.o
endif
ifdef CONFIG_QTI_RPMH_API
	obj-$(CONFIG_QTI_RPM_STATS_LOG) += rpmh_master_stat.o
endif
obj-$(CONFIG_QCOM_SMCINVOKE) += smcinvoke.o
obj-$(CONFIG_QMP_DEBUGFS_CLIENT) += qmp-debugfs-client.o
obj-$(CONFIG_MSM_REMOTEQDSS) += remoteqdss.o
obj-$(CONFIG_QSEE_IPC_IRQ_BRIDGE) += qsee_ipc_irq_bridge.o
obj-$(CONFIG_QCOM_QDSS_BRIDGE) += qdss_bridge.o
obj-$(CONFIG_MSM_BAM_DMUX) += bam_dmux.o
obj-$(CONFIG_WCNSS_CORE) += wcnss/
obj-$(CONFIG_BIG_CLUSTER_MIN_FREQ_ADJUST) += big_cluster_min_freq_adjust.o

# SHARP Device Drivers for Qualcomm SOC
obj-y += sharp/
