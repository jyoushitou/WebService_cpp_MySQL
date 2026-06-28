//MyMySQL.h工具实现文件
// 使用连接池管理数据库连接

#include "MyMySQL.h"

namespace MySQL{
    //构造函数
    mysql::mysql(){
        database = "web_server";                        // 默认数据库名
        // 连接池由 PoolManager 统一管理，构造函数不再直接创建连接
        PoolManager::Instance().Initialize();
        Utils::Out_System_Mysql("mysql 对象已创建，使用连接池模式");
    }

    mysql::mysql(std::string str){
        database = str;                                 // 设置数据库名
        PoolManager::Instance().Initialize();
        Utils::Out_System_Mysql("mysql 对象已创建，使用连接池模式，数据库: " + str);
    }
    
    //内部工具（兼容旧接口，但实际不再直接使用）
    bool mysql::Create_Socket(){
        // 连接池模式下不再需要单独检查句柄
        return 0;
    }

    bool mysql::Connect(){
        // 连接池模式下不再需要单独连接
        return 0;
    }

    bool mysql::Close(){
        // 连接池模式下不再需要单独关闭
        return 0;
    }

    bool mysql::Set_Databases(std::string str){
        // 数据库选择在获取连接时由连接池处理
        database = str;
        Utils::Out_System_Mysql("已设置数据库: " + database);
        return 0;
    }

    bool mysql::Set_UTF8(){
        // 字符集在连接池创建连接时已设置
        return 0;
    }

    // 外部可使用函数
    // 数据库管理
    bool mysql::Create_DataBases(){
       return Create_DataBases(database);
    }

    bool mysql::Create_DataBases(std::string str){
        // 从连接池获取连接
        MYSQL* conn = GetConnection();
        if (!conn) {
            Utils::Out_System_Error("获取数据库连接失败");
            return 1;
        }
        
        std::string sql = "CREATE DATABASE IF NOT EXISTS " + str;
        bool result = 0;
        if (mysql_query(conn, sql.c_str())) {
            Utils::Out_System_Error("创建数据库失败: " + std::string(mysql_error(conn)));
            result = 1;
        } else {
            Utils::Out_System_Mysql("数据库 " + str + " 已就绪");
            if(database != str){
                database = str;
                Utils::Out_System_Mysql("数据库"+database+"已经设置为默认数据库");
            }
            result = 0;
        }
        
        ReleaseConnection(conn);
        return result;
    }

    // 用户登录鉴权，查询 users 表匹配用户名密码，返回权限级别
    int mysql::User(const std::string name , const std::string user_password, std::string* userAvatar){
        // 从连接池获取连接
        MYSQL* conn = GetConnection();
        if (!conn) {
            Utils::Out_System_Error("获取数据库连接失败");
            return -1;
        }
        
        // 确保使用正确的数据库
        if (!database.empty()) {
            mysql_select_db(conn, database.c_str());
        }
        
        mysql_query(conn, "SELECT DATABASE();");
        MYSQL_RES* dbRes = mysql_store_result(conn);
        if (dbRes) {
            MYSQL_ROW dbRow = mysql_fetch_row(dbRes);
            std::cout << "[MySQL Debug] Current database: " << (dbRow ? dbRow[0] : "NULL") << std::endl;
            mysql_free_result(dbRes);
        }

        // ⚠️ SQL 注入风险：name 如果包含单引号可被利用
        std::string sql = "select name, password, permission, avatar from users where name = '" + name + "';";
        std::cout << "[MySQL Debug] SQL: " << sql << std::endl;
        if (mysql_query(conn, sql.c_str())) {
            Utils::Out_System_Error("查询失败: " + std::string(mysql_error(conn)));
            ReleaseConnection(conn);
            return -1;
        }
        MYSQL_RES* result = mysql_store_result(conn);

        if(!result){
            Utils::Out_System_Error("获取结果失败: " + std::string(mysql_error(conn)));
            ReleaseConnection(conn);
            return -1;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        if(!row){
            Utils::Out_System_Mysql("未找到用户");
            mysql_free_result(result);
            ReleaseConnection(conn);
            return 0;
        }

        // 解析结果行：row[0]=name, row[1]=password, row[2]=permission, row[3]=avatar
        std::string db_password = row[1] ? row[1] : "";
        int db_permission = 0;
        if (row[2]) {
            db_permission = std::stoi(row[2]);
        }

        int ret;
        if (db_password == user_password) {
            Utils::Out_System_Mysql("欢迎用户 " + name + "，权限: " + std::to_string(db_permission));
            if (userAvatar != nullptr && row[3]) {
                *userAvatar = row[3];
            }
            ret = db_permission;
        } 
        else {
            Utils::Out_System_Mysql("密码错误");
            ret = 0;
        }
        
        mysql_free_result(result);
        ReleaseConnection(conn);
        return ret;
    }

    // 根据用户名查询用户 ID
    int mysql::Get_UserId(std::string name){
        MYSQL* conn = GetConnection();
        if (!conn) {
            Utils::Out_System_Error("获取数据库连接失败");
            return -1;
        }
        
        if (!database.empty()) {
            mysql_select_db(conn, database.c_str());
        }
        
        std::string sql = "select id from users where name = '" + name + "';";
        if (mysql_query(conn, sql.c_str())) {
            Utils::Out_System_Error("查询用户ID失败: " + std::string(mysql_error(conn)));
            ReleaseConnection(conn);
            return -1;
        }
        MYSQL_RES* result = mysql_store_result(conn);
        if(!result){
            Utils::Out_System_Error("获取结果失败: " + std::string(mysql_error(conn)));
            ReleaseConnection(conn);
            return -1;
        }
        MYSQL_ROW row = mysql_fetch_row(result);
        if(!row){
            Utils::Out_System_Mysql("未找到用户");
            mysql_free_result(result);
            ReleaseConnection(conn);
            return 0;
        }
        int user_id = row[0] ? std::stoi(row[0]) : 0;
        mysql_free_result(result);
        ReleaseConnection(conn);
        return user_id;
    }

    // 根据用户名查询用户头像
    std::string mysql::Get_Avatar(std::string name){
        MYSQL* conn = GetConnection();
        if (!conn) {
            Utils::Out_System_Error("获取数据库连接失败");
            return "";
        }
        
        if (!database.empty()) {
            mysql_select_db(conn, database.c_str());
        }
        
        std::string sql = "select avatar from users where name = '" + name + "';";
        if (mysql_query(conn, sql.c_str())) {
            Utils::Out_System_Error("查询头像失败: " + std::string(mysql_error(conn)));
            ReleaseConnection(conn);
            return "";
        }
        MYSQL_RES* result = mysql_store_result(conn);
        if(!result){
            Utils::Out_System_Error("获取头像结果失败: " + std::string(mysql_error(conn)));
            ReleaseConnection(conn);
            return "";
        }
        MYSQL_ROW row = mysql_fetch_row(result);
        if(!row){
            mysql_free_result(result);
            ReleaseConnection(conn);
            return "";
        }
        std::string avatar = row[0] ? row[0] : "";
        mysql_free_result(result);
        ReleaseConnection(conn);
        return avatar;
    }

    // 析构函数：连接由连接池管理，无需关闭
    mysql::~mysql(){
        Utils::Out_System_Mysql("mysql 对象析构，连接已归还连接池");
    }

    // 全局函数：创建数据库（使用已有连接句柄）
    bool Create_DataBases(MYSQL* conn, std::string str){
        std::string sql = "CREATE DATABASE IF NOT EXISTS " + str;
        if (mysql_query(conn, sql.c_str())) {
            Utils::Out_System_Error("创建数据库失败: " + std::string(mysql_error(conn)));
            return 1;
        }
        return 0;
    }
}