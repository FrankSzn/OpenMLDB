package com._4paradigm.sql.sdk;

public class SdkOption {
    private String zkCluster;
    private String zkPath;
    private long sessionTimeout;
    private Boolean enableDebug = false;

    public String getZkCluster() {
        return zkCluster;
    }

    public void setZkCluster(String zkCluster) {
        this.zkCluster = zkCluster;
    }

    public String getZkPath() {
        return zkPath;
    }

    public void setZkPath(String zkPath) {
        this.zkPath = zkPath;
    }

    public long getSessionTimeout() {
        return sessionTimeout;
    }

    public void setSessionTimeout(long sessionTimeout) {
        this.sessionTimeout = sessionTimeout;
    }

    public Boolean getEnableDebug() {
        return enableDebug;
    }

    public void setEnableDebug(Boolean enableDebug) {
        this.enableDebug = enableDebug;
    }
}