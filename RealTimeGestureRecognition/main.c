#include <stdio.h>
#include <stdlib.h>
#include "PktParser.h"
#include "SerialPort.h"
#include "DataNode.h"
#include "FileUtil.h"
#include "SPRING.h"
#include <string.h>

int main(int argc, char *argv[]) {

	char portId[5];

	char gszPort[20];   //The port number for serial communication, please change it to the right one.

	printf("input the port COM number: \n");
	scanf("%s", &portId);

	strcpy(gszPort, "\\\\.\\com");
	strcat(gszPort, portId);

	printf("port Id: %s\n", gszPort);



	if (openPort(gszPort)) {

		if (setupPort()) {

			PktData zeroPacket = {0.0, 0.0};
			PktData pktData;
			///////////////////////////////////////////////////////////////
			//Start initialization

			//Mag data list for initialization
			DataHeadNode *ptr = create_list_with_head();

			int dataNum;
			printf("Input the count of data need to be collected, at least 3000 for calibration. \n");
			scanf("%d", &dataNum);

			//Before read, flush the buffer.
			purgePort();

			//Collect data for initialization
			int i = 0;
			while (i < dataNum) {
				pktData = blockingReadOnePacket();

				if(equals(pktData, zeroPacket)) {
					printf("ignore zero data\n");
					continue;
				}
				add_to_list_head(ptr, pktData);
				i++;
			}

			//Start prepare double array for initialize calibrator
			int len = ptr->length;
			printf("Initialize data length: %d \n", len);

			double magDataX[len] ;
			double magDataY[len] ;
			double magDataZ[len] ;

			fillMagDataArray(ptr, magDataX, magDataY, magDataZ);

			//pass magData array to calibrator
			initializeCalibrator(magDataX, magDataY, magDataZ, len);

			//clear data
			clear_list(ptr);


			///////////////////////////////////////////////////////////////
			//Start doing tests!!

			int testTimes;
			printf("======================= Now can can collect test data!! =================== \n\nHow many times do you want to collect data for calibration? \n");
			scanf("%d", &testTimes);

			SqQueue * queue = create_empty_queue();

			int magDataNum = 0;
			int magLen = 0;

			char rawDataFileName[60];  			//The file stores raw data
			char correctedDataFileName[60];  	//The file stores corrected magnetic data

			int j;
			for(j = 0; j < testTimes; j ++) {
				printf("========================  THE %d TEST  ====================== \n", (j + 1));
				sprintf(rawDataFileName, "C:/Users/xing/Desktop/%d_Raw_Mag_Data.txt",j);
				sprintf(correctedDataFileName, "C:/Users/xing/Desktop/%d_Corrected_Mag_Data.txt",j);
				printf("\nInput the count of mag data need to be collected this time (No more than %d): \n", MAX_SIZE);

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
				for(gt = 0; gt < DTW_NUM; gt++)
                {
                    og[gt] = read_file_to_init_original_gesture(gestureModel[gt]);
                    int m = og[gt]->m;
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
                    for(k = 0; k <= m; k++)
                    {
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

				scanf("%d", &magDataNum);

				//Before read, flush the buffer.
				purgePort();

                int trueNum = 0;
                bool hasTarget = false;
                DataHeadNode *targetHead = create_list_with_head();

				for(i = 0; i < magDataNum; i ++) {
					pktData = blockingReadOnePacket();
					if(equals(pktData, zeroPacket)) {
						//	printf("ignore zero data\n");
						continue;
					}
					//Notice: it will override original raw data if queue is full
					int position = add_to_queue(queue, pktData);

					//input the current data into the SPRING

                    if(SPRING(pktData, &grp[0],position, queue) == TARGET_TYPE)
                    {
                        trueNum++;
                        add_to_list_head(targetHead, pktData);
                    }


                    if(trueNum >= 20)
                    {
                        /** compute target using the list of the target data list */

                        printf("\n!!!!!!!!!!!!!!!!!!!!!!target selected!!!!!!!!!!!!!!!!!!\n");
                        trueNum = 0;
                        clear_list(targetHead);
                        hasTarget = true;
                    }

                    //if(hasTarget)
                    {
                        int l = 0;
                        for(l = 7; l <= 7; l++)
                        {
                            SPRING(pktData, &grp[l],position, queue);
                        }
                    }

                    //compare_list_and_delete_queue(queue,startList,4);
				}

				magLen = get_queue_length(queue);
				printf("Actual mag data length: %d\n", magLen);

				//This is the same heading in Cali.mat
				double heading[magLen];

				write_queue_to_file(rawDataFileName, queue);

				calibrateMagData(queue->magXData, queue->magYData, queue->magZData, heading, magLen);

				write_mag_to_file(correctedDataFileName, queue->magXData, queue->magYData, queue->magZData, heading, magLen);

				clear_queue(queue);
				printf("\nSee %s \nand %s\nfor more detail!\n",rawDataFileName, correctedDataFileName);
			}

			free_queue(queue);

			clearCalibrator();
		}

		closePort();
	}


//	print_pktData(get_element_from_head(ptr));
//	print_pktData(get_element_from_end(ptr));
//
//	delete_from_list_head(ptr);
//	delete_from_list_end(ptr);
//	print_list(ptr);
//
//	delete_from_list_end(ptr);
//	delete_from_list_head(ptr);
//	print_list(ptr);
//
//	delete_from_list_head(ptr);
//	delete_from_list_end(ptr);
//	print_list(ptr);

	system("pause");
	return 0;
}



