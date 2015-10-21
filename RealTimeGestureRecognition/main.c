#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <process.h>
#include <time.h> //time_t time()

#include "PktParser.h"
#include "SerialPort.h"
#include "DataNode.h"
#include "DataCalibrator.h"
#include "FileUtil.h"
#include "SPRING.h"
#include <string.h>

#define MAG_CALI_TIME 5 //the time need to collect initial data, in seconds.

const PktData ZERO_PKT = {0.0, 0.0};

//used to only initialize calibrator once.
bool isCalibratorBeingInitialized = false;

bool isCalibratorInitialized = false;

//1. collect initial data in MAG_CALI_TIME seconds
//2. do calibration for initial data
//3. if calibrator is invalid, repeat step 1.
void initCalibrator(HANDLE hComm) {
    printf("\n=========================  Initialize Calibrator  =====================\n");
    PktData pktData;

    //Mag data list for initialization
    DataHeadNode *ptr = create_list_with_head();
    int len;

    while (true) {
        printf("\nCollect data in the next %d seconds!\n", MAG_CALI_TIME);

        //Before read, flush the buffer.
        purgePort(hComm);

        time_t timeBegin = time(NULL);

        while (true) {
            pktData = blockingReadOnePacket(hComm);

            if(equals(pktData, ZERO_PKT)) {
                continue;
            }
            add_to_list_head(ptr, pktData);

            if(time(NULL) - timeBegin >= MAG_CALI_TIME)
                break;
        }

        //Start prepare double array for calculate calibrator
        len = ptr->length;
        printf("Initialize data length: %d \n", len);

        double magDataX[len] ;
        double magDataY[len] ;
        double magDataZ[len] ;
        double heading[len];

        fillMagDataArray(ptr, magDataX, magDataY, magDataZ);

        //pass magData to calibrator
        calculateCalibrator(magDataX, magDataY, magDataZ, len);

        write_list_to_file("C:/Users/xing/Desktop/Raw_Initial_Data.txt", ptr);

        if(! calibrateMagData(magDataX, magDataY, magDataZ, heading, len))
            continue;

        write_mag_to_file("C:/Users/xing/Desktop/Corrected_Initial_Mag_Data.txt", magDataX, magDataY, magDataZ, heading, len);

        clear_list(ptr);

        if(isCalibratorValid(magDataX, magDataY, magDataZ, len))
            break;
    }

    //free all list data
    free_list(ptr);
    isCalibratorInitialized = true;
    printf("\n============================  Initialize Over  ========================\n");
}

void ThreadFunc(Params* params) {
    printf("=========================================================================\n");
    printf("                 SubThread %ld is watching over %s port \n", GetCurrentThreadId(), params->gszPort);
    printf("=========================================================================\n");

    HANDLE hComm = openPort(params->gszPort);
    if (hComm == INVALID_HANDLE_VALUE) {
        printf("failed to open serial port %s \n", params->gszPort);
        return;
    } else {
        printf("serial port %s opened \n", params->gszPort);

        if (setupPort(hComm)) {
            // all sensors use the same calibration matrix and offset
            while(isCalibratorInitialized == false) {
                if(isCalibratorBeingInitialized == true)
                    continue;
                isCalibratorBeingInitialized = true;
                printf("\n************** please rotate your sensor with port %s *************\n", params->gszPort);
                initCalibrator(hComm);
            }

            printf("======================= Now collect test data!! =================== \n");

            SqQueue * queue = create_empty_queue();

            char rawDataFileName[60];  			//The file stores raw data
            char correctedDataFileName[60];  	//The file stores corrected magnetic data

            printf("========================  THE TEST  ====================== \n");
            sprintf(rawDataFileName, "C:/Users/xing/Desktop/%s_Raw_Mag_Data.txt",params->gszPort);
            sprintf(correctedDataFileName, "C:/Users/xing/Desktop/%s_Corrected_Mag_Data.txt",params->gszPort);


            //the models of the four gestures
            char *gestureModel[DTW_NUM] = {"./gesture_model/target.txt"
                ,"./gesture_model/point.txt"
                ,"./gesture_model/rotate_right.txt"
                ,"./gesture_model/rotate_left.txt"
                ,"./gesture_model/slide_over.txt"
                ,"./activity_model/stand_up.txt"
                ,"./activity_model/sit_down.txt"
                ,"./activity_model/walk.txt"};

            //the tresholds of four model gestures
            double threshold[DTW_NUM] = {TARGET_THRESHOLD,POINT_THRESHOLD,ROTATE_RIGHT_THRESHOLD,ROTATE_LEFT_THRESHOLD
                ,SLIDE_OVER_THRESHOLD,STAND_UP_THRESHOLD,SIT_DOWN_THRESHOLD,WALK_THRESHOLD};

            //the time limit of four model gestures
            double timeLimit[DTW_NUM] = {TARGET_TIMELIMIT,POINT_TIMELIMIT,ROTATE_RIGHT_TIMELIMIT,ROTATE_LEFT_TIMELIMIT
                ,SLIDE_OVER_TIMELIMIT,STAND_UP_TIMELIMIT,SIT_DOWN_TIMELIMIT,WALK_TIMELIMIT};

            //initialize the four models and their GestureRecognitionProcess
            //the order is :
            //0->point
            //1->rotate right
            //2->rotate left
            //3->slide over
            OriginalGesture *og[DTW_NUM];
            GRProcess grp[DTW_NUM];
            int gt = 0;
            for(gt = 0; gt < DTW_NUM; gt++) {
                og[gt] = read_file_to_init_original_gesture(gestureModel[gt]);
                int m = og[gt]->m;
                //Pay attention to Free memory !!!
                double *distanceArray = (double *)malloc(sizeof(double) * (m + 1));
                double *distanceArrayLast = (double *)malloc(sizeof(double) * (m + 1));
                int *startArray = (int *)malloc(sizeof(int) * (m + 1));
                int *startArrayLast = (int *)malloc(sizeof(int) * (m + 1));
                long int *timeArray = (long int *)malloc(sizeof(long int) * (m + 1));
                long int *timeArrayLast = (long int *)malloc(sizeof(long int) * (m + 1));
                double dmin = DBL_MAX;
                int te = 1;
                int ts = 1;
                int k = 0;
                for(k = 0; k <= m; k++) {
                    distanceArrayLast[k] = DBL_MAX;
                    startArrayLast[k] = 0;
                    timeArrayLast[k] = 0;
                }
                grp[gt].distanceArray = distanceArray;
                grp[gt].distanceArrayLast = distanceArrayLast;
                grp[gt].dmin = dmin;
                grp[gt].originalGesture = *(og[gt]);
                grp[gt].startArray = startArray;
                grp[gt].startArrayLast = startArrayLast;
                grp[gt].timeArray = timeArray;
                grp[gt].timeArrayLast = timeArrayLast;
                grp[gt].threshold = threshold[gt];
                grp[gt].te = te;
                grp[gt].ts = ts;
                grp[gt].times = 0;
                grp[gt].times = 0;
                grp[gt].type = gt;
                grp[gt].timeLimit = timeLimit[gt];
            }

            //Before read, flush the buffer.
            purgePort(hComm);

            int trueNum = 0;
            bool hasTarget = false;
            DataHeadNode *targetHead = create_list_with_head();

            PktData pktData;
            int i;
            for(i = 0; i < params->magDataNum; i ++) {
                pktData = blockingReadOnePacket(hComm);
                if(equals(pktData, ZERO_PKT)) {
                    continue;
                }
                //Notice: it will override original raw data if queue is full
                int position = add_to_queue(queue, pktData);

                //input the current data into the SPRING
                if(SPRING(pktData, &grp[0],position, queue) == TARGET_TYPE) {
                    trueNum++;
                    add_to_list_head(targetHead, pktData);
                }


                if(trueNum >= 20) {
                    /** compute target using the list of the target data list */

                    printf("\n!!!!!!!!!!!!!!!!!!!!!!%s target selected!!!!!!!!!!!!!!!!!!\n", params->gszPort);
                    trueNum = 0;
                    clear_list(targetHead);
                    hasTarget = true;
                }

                if(hasTarget) {
                    int l = 0;
                    for(l = 1; l <= 4; l++) {
                        SPRING(pktData, &grp[l],position, queue);
                    }
                }

                //compare_list_and_delete_queue(queue,startList,4);
            }

            int magLen = get_queue_length(queue);
            printf("Actual mag data length: %d\n", magLen);

            //This is the same heading in Cali.mat
            double heading[magLen];

            write_queue_to_file(rawDataFileName, queue);

            calibrateMagData(queue->magXData, queue->magYData, queue->magZData, heading, magLen);

            write_mag_to_file(correctedDataFileName, queue->magXData, queue->magYData, queue->magZData, heading, magLen);

            //clear_queue(queue);
            printf("\nSee %s \nand %s\nfor more detail!\n",rawDataFileName, correctedDataFileName);

            free_queue(queue);

            free_list(targetHead);
        }
        closePort(hComm);
    }
}


int main(int argc, char *argv[]) {

    printf("how many sensors do you have :\n");
    int portCount = 0;
    scanf("%d", &portCount);

    HANDLE handle[portCount];
    Params params[portCount];

    int portId;
    int i;
    for(i = 0; i < portCount; i ++) {
        Params param;

        printf("input the port COM number: \n");
        scanf("%d", &portId);
        sprintf(param.gszPort, "\\\\.\\com%d" ,portId);

        printf("Input the count of mag data need to be collected this time :\n");
        scanf("%d", &param.magDataNum);

        params[i] = param;

        //printf("Param %d : %s,  %d \n", i, params[i].gszPort, params[i].magDataNum);
    }

    for(i = 0; i < portCount; i ++)
        handle[i] = (HANDLE) _beginthreadex(NULL, 0, ThreadFunc, &(params[i]), 0, NULL);

    //wait until all sub threads end
    WaitForMultipleObjects(portCount, handle, TRUE, INFINITE);

    clearCalibrator();

    system("pause");
    return 0;
}



