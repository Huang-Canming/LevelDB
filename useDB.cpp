#include <cassert>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include "./LevelDB/include/leveldb/db.h"   // 注意文件放的位置，应该要和leveldb的大文件夹放在同一级

using namespace std;
using namespace leveldb;

int main() {
    int iKey = 100;
    string strKey;
    string strValue;
    char cstrTmp[10] = {0};
		
    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status status;

    // 配置项：如果LevelDB数据库目录不存在，则自动创建
    options.create_if_missing = true;

    status = leveldb::DB::Open(options, "/root/testdb", &db);
    assert(status.ok());

	while (iKey < 100000) {
	    iKey++;
		
        
        snprintf(cstrTmp, 10, "%d", iKey);
        strKey = cstrTmp;
	    strValue = cstrTmp;

        cout << strKey << endl;

	    // 将key/value键值对写入LevelDB数据库
	    status = db->Put(leveldb::WriteOptions(), strKey, strValue);
	}
	
    std::string get;
    // 写入成功后，尝试根据key进行检索
    if(status.ok()) {
        status = db->Get(leveldb::ReadOptions(), "100000", &get);
    }
    if(status.ok()) {
        std::cout << "读取到Key=" << strKey << "对应的Value=" << get << "." << std::endl;
    } else {
        std::cout << "读取失败！" << std::endl;
    }

    delete db;
    return 0;
}