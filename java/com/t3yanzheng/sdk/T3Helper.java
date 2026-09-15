package com.t3yanzheng.sdk;

/**
 * T3 验证辅助类
 * 封装 T3 SDK 的初始化、登录、心跳流程
 *
 * 验证核心逻辑全部在 libt3sdk.so 中实现（通过 JNI 调用）。
 *
 * 注意: 下面的参数为占位符，请替换为你自己的 T3 后台配置。
 *       登录 T3 后台 (https://www.t3yanzheng.com) 创建应用后可获取:
 *       - 登录调用码 (login)
 *       - 公告调用码 (notice)
 *       - 版本调用码 (version)
 *       - 心跳调用码 (heartbeat)
 *       - APPKEY
 *       - 自定义 Base64 字符集 (64 个字符)
 */
public class T3Helper {

    // ====== 请替换为你的 T3 后台配置 ======
    private static final String LOGIN_CODE     = "你的登录调用码";
    private static final String NOTICE_CODE    = "你的公告调用码";
    private static final String VERSION_CODE   = "你的版本调用码";
    private static final String HEARTBEAT_CODE = "你的心跳调用码";
    private static final String APPKEY         = "你的APPKEY";
    private static final String BASE64_CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    // ======================================

    /**
     * 执行卡密验证（核心逻辑在 libt3sdk.so 中）
     * @param kami 卡密
     * @return 验证结果，[0]=成功标志("1"成功/"0"失败), [1]=消息, [2]=statecode(成功时用于心跳)
     */
    public static String[] verify(String kami) {
        T3Verify t3 = new T3Verify();
        try {
            boolean initOk = t3.init(LOGIN_CODE, NOTICE_CODE, VERSION_CODE,
                    HEARTBEAT_CODE, APPKEY, BASE64_CHARSET);
            if (!initOk) {
                return new String[]{"0", "SDK初始化失败，请检查调用码和Base64字符集(必须64字符)", ""};
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
            t3.init(LOGIN_CODE, NOTICE_CODE, VERSION_CODE,
                    HEARTBEAT_CODE, APPKEY, BASE64_CHARSET);
            T3Result result = t3.heartbeat(kami, statecode);
            return result.success;
        } catch (Throwable t) {
            return false;
        } finally {
            t3.destroy();
        }
    }
}
