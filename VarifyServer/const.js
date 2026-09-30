let code_prefix = "code_";

const Errors = {
    Success : 0,
    RedisErr : 1,
    Exception : 2,
    UserExist : 3,      // 用户名已存在
    EmailExist : 4,     // 邮箱已存在
    InvalidParams : 5,  // 无效的参数
};

module.exports = {code_prefix,Errors}