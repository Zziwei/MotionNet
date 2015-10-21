#ifndef FILEUTIL_H
#define FILEUTIL_H

#include<stdio.h>
#include "DataNode.h"
#include "SPRING.h"

OriginalGesture *read_file_to_init_original_gesture(char * fileName);


//Write packet data to file.
void write_pkt_to_file(char * fileName, PktData pktData);

//Write magnetic data to file.
void write_mag_to_file(char * fileName, double x[], double y[], double z[], double heading[], int len) ;

#endif // FILEUTIL_H
