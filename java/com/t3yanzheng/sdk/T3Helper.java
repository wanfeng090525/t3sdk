package com.t3yanzheng.sdk;

/**
 * T3 验证辅助类
 *
 * 验证核心逻辑 + T3 后台凭证全部编译在 libt3sdk.so 中。
 * 本类只负责调用流程，不包含任何凭证。
 *
 * 如需修改 T3 后台凭证（调用码/APPKEY 等），
 * 请编辑 t3sdk_jni.c 顶部的 T3_* 宏定义后重新编译 .so。
 */
public class T3Helper {

    /**
     * 执行卡密验证（核心逻辑 + 凭证均在 libt3sdk.so 中）
     * @param kami 卡密
     * @return 验证结果，[0]=成功标志("1"成功/"0"失败), [1]=消息, [2]=statecode(成功时用于心跳)
     */
    public static String[] verify(String kami) {
        T3Verify t3 = new T3Verify();
        try {
            boolean initOk = t3.init();
            if (!initOk) {
                return new String[]{"0", "SDK初始化失败，请检查 .so 中的凭证配置", ""};
            }

            if (kami == null || kami.trim().length() == 0) {
                return new String[]{"0", "请输入卡密", ""};
            }

            String imei = T3Verify.getMachineCode();
            T3LoginResult result = t3.login(kami.trim(), imei);

            if (result.success) {
                String msg = "验证成功\n卡密ID: " + result.id +
                        "\n到期时间: " + result.endTime +
                        "\n核心数据: " + result.core;
                return new String[]{"1", msg, result.statecode};
            } else {
                return new String[]{"0", "验证失败: " + result.error, ""};
            }
        } catch (Throwable t) {
            return new String[]{"0", "验证异常: " + t.getMessage(), ""};
        } finally {
            t3.destroy();
        }
    }

    /**
     * 心跳验证（核心逻辑在 libt3sdk.so 中）
     * @param kami 卡密
     * @param statecode 登录返回的状态码
     * @return true 心跳成功
     */
    public static boolean heartbeat(String kami, String statecode) {
        T3Verify t3 = new T3Verify();
        try {
            t3.init();
            T3Result result = t3.heartbeat(kami, statecode);
            return result.success;
        } catch (Throwable t) {
            return false;
        } finally {
            t3.destroy();
        }
    }
}
