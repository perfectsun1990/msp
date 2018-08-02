
#include "alphaGo.hpp"
#include <vector>
/*************************************************************************
* @file   : EncodeDecode.c
* @author : sun
* @mail   : perfectsun1990@163.com
* @version: v1.0.0
* @date   : 2018年08月02日 星期四 10时26分12秒
* Copyright (C), 1990-2020, Tech.Co., Ltd. All rights reserved.
************************************************************************/

#include "encrypt.hpp"

int32_t main(int32_t argc, char** argv)
{
	DoEncodeDecode("E:\\av-test\\test.ccr", "E:\\av-test\\test-encode.ccr");
	DoEncodeDecode("E:\\av-test\\test-encode.ccr", "E:\\av-test\\test-decode.ccr");
	system("pause");
}
