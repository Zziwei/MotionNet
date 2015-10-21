#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <stdio.h>
#include <stdbool.h>
#include <Windows.h>	//Contain definations for handle, DWORD and DCB
#include <tchar.h>		//Contain definations for _T

//int portId;				//e.g Com#3 's portId is 3.
//TCHAR commPort[11];		//e.g "\\\\.\\com3"

/*
    Definitions for the methods about serial port operations.
*/
bool openPort(TCHAR *gszPort);

bool setupPort();

bool purgePort();

bool closePort();

bool sendSByte(unsigned char byteToWrite);

unsigned char readSByte();

#endif  //SERIALPORT_H
