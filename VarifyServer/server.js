const grpc = require('@grpc/grpc-js')
const message_proto = require('./proto')
const { v4: uuidv4 } = require('uuid');
const emailModule = require("./email")
const const_module = require("./const")
const redis_module = require("./redis")
const config_module = require("./config")

async function GetVarifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try {
        let query_res = await redis_module.GetRedis(const_module.code_prefix + call.request.email);
        console.log("query_res is ", query_res)
        
        let uniqueId = query_res;
        if (query_res == null) {
            uniqueId = uuidv4();
		
            if (uniqueId.length > 4) {
                uniqueId = uniqueId.substring(0, 4);
            }
			uniqueId = uniqueId.toUpperCase();
            let bres = await redis_module.SetRedisExpire(const_module.code_prefix + call.request.email, uniqueId, 600)
            if (!bres) {
                callback(null, {
                    email: call.request.email,
                    error: const_module.Errors.RedisErr
                });
                return;
            }
        }

        console.log("uniqueId is ", uniqueId)

        // --- 1. 定义纯文本内容 (作为备用) ---
        let text_str = `[FoolChat] 您的验证码是 ${uniqueId}。请在10分钟内完成注册。如非本人操作，请忽略此邮件。`;

        // --- 2. 定义美化后的 HTML 内容 ---
        let html_str = `
            <div style="font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif; min-width: 320px; max-width: 600px; margin: 0 auto; background-color: #f9f9f9; padding: 20px; border-radius: 10px;">
                <div style="background-color: #ffffff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.05);">
                    <div style="text-align: center; margin-bottom: 20px;">
                        <h1 style="color: #333333; margin: 0; font-size: 36px;">FoolChat</h1>
                    </div>
                    
                    <p style="color: #555555; font-size: 16px; line-height: 1.5; margin-bottom: 20px;">
                        亲爱的用户，您好：
                    </p>
                    
                    <p style="color: #555555; font-size: 16px; line-height: 1.5; margin-bottom: 20px;">
                        您正在注册 <strong>FoolChat</strong> 账号。请使用以下验证码完成验证：
                    </p>
                    
                    <div style="text-align: center; margin: 30px 0;">
                        <span style="display: inline-block; background-color: #e8f0fe; color: #1a73e8; font-size: 28px; font-weight: bold; letter-spacing: 4px; padding: 15px 30px; border-radius: 6px; border: 1px solid #d2e3fc;">
                            ${uniqueId}
                        </span>
                    </div>
                    
                    <p style="color: #888888; font-size: 14px; line-height: 1.5; margin-bottom: 10px;">
                        ⚠️ <strong>注意：</strong>
                    </p>
                    <ul style="color: #888888; font-size: 14px; line-height: 1.5; padding-left: 20px;">
                        <li>该验证码将在 <strong>10分钟</strong> 后失效。</li>
                        <li>如果这不是您的操作，请忽略此邮件，您的账号将不会受到影响。</li>
                        <li>请勿将验证码告知他人。</li>
                    </ul>

                    <div style="border-top: 1px solid #eeeeee; margin-top: 30px; padding-top: 20px; text-align: center;">
                        <p style="color: #aaaaaa; font-size: 12px;">
                            © ${new Date().getFullYear()} FoolChat Team. All rights reserved.
                        </p>
                    </div>
                </div>
            </div>
        `;

        // --- 3. 更新邮件选项 ---
        let mailOptions = {
            from: `"验证码服务" <${config_module.email_user}>`,
            to: call.request.email,
            subject: '【FoolChat】注册验证码', // 优化标题，使其看起来更正规
            text: text_str,      // 某些不支持HTML的客户端会显示这个
            html: html_str,      // 支持HTML的客户端会显示这个
        };

        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        if (!send_res) {
            callback(null, {
                email: call.request.email,
                error: const_module.Errors.RedisErr
            });
            return;
        }
        callback(null, {
            email: call.request.email,
            error: const_module.Errors.Success
        });

    } catch (error) {
        console.log("catch error is ", error)
        callback(null, {
            email: call.request.email,
            error: const_module.Errors.Exception
        });
    }
}

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VarifyService.service, { GetVarifyCode: GetVarifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')        
    })
}

main()