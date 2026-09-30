// mysql.js
const mysql = require('mysql2/promise');
const config_module = require("./config");

// 创建 MySQL 连接池
const pool = mysql.createPool({
    host: config_module.mysql_host,
    port: config_module.mysql_port,
    user: config_module.mysql_user,
    password: config_module.mysql_passwd,
    database: config_module.mysql_database,
    waitForConnections: true,
    connectionLimit: 10,
    queueLimit: 0
});

/**
 * 检查用户名是否存在
 * @param {string} username 
 * @returns {Promise<boolean>}
 */
async function checkUsernameExists(username) {
    try {
        const [rows] = await pool.execute(
            'SELECT id FROM user WHERE username = ?',
            [username]
        );
        return rows.length > 0;
    } catch (error) {
        console.error('checkUsernameExists error:', error);
        throw error;
    }
}

/**
 * 检查邮箱是否存在
 * @param {string} email 
 * @returns {Promise<boolean>}
 */
async function checkEmailExists(email) {
    try {
        const [rows] = await pool.execute(
            'SELECT id FROM user WHERE email = ?',
            [email]
        );
        return rows.length > 0;
    } catch (error) {
        console.error('checkEmailExists error:', error);
        throw error;
    }
}

module.exports = {
    checkUsernameExists,
    checkEmailExists
};