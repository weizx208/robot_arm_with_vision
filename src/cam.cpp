#include "cam.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <windows.h>
#include <unistd.h>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <list>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco/charuco.hpp>

using namespace cv;
using namespace std;

cam::cam(uint8_t cameraNum, int framesPerSecond){
    camera = cameraNum;
    fps = framesPerSecond;
}

int cam::findIndex(vector<int>& vec, int val){
    int res;
    uint16_t length = vec.size();
    res = find(vec.begin(), vec.end(), val) - vec.begin();
        if (res >= length){
            res = -1;
        }
    return res;
}

void cam::findRelativeVectors(int basePos, int Pos, vector<Vec3d>& translationVectors, vector<Vec3d>& rotationVectors, vector<double>& posRes, vector<double>& Rotres ){
    int i,j;
    vector<double> R(3);
    Mat baseRotMatrix = Mat::eye(3,3, CV_64F);
//    Mat posRotMatrix = Mat::eye(3,3, CV_64F);

    /* posRes is the vector from the world coordinate system to object 1 expressed in world base vectors*/
    /* R is the vector from object to base in expressed in the camera frame*/
    for(i = 0; i < 3; i++)
        R[i] = translationVectors[Pos][i] -  translationVectors[basePos][i];

    /* R is still expressed with respect to the camera frame, to fix this we must multiply R by the transpose of the rotation matrix between the world and camera frame */
    Rodrigues(rotationVectors[basePos],baseRotMatrix);

    for(i = 0; i < 3; i++){
        posRes[i] = 0;
        for(j = 0; j < 3; j++){
            posRes[i] += baseRotMatrix.at<double>(j,i)*R[j];
        }
    }

//
//    Rodrigues(rotationVectors[basePos],baseRotMatrix);
//    Rodrigues(rotationVectors[Pos1],posRotMatrix);



    /* does this work as expected?*/
    for(i = 0; i < 3; i++)
        Rotres[i] = rotationVectors[Pos][i] - rotationVectors[basePos][i];
    /* Using rotation matrices, multiply one by the transpose (inverse rotation) of the other one. But maybe the above works as, well who knows. */
}

int cam::startWebcamMonitoring(const Mat& cameraMatrix, const Mat& distanceCoefficients, float arucoSquareDimension, vector<double>& relPos1, vector<double>& relRot1, int baseMarker, int toFindMarker){
    vector<double> tempRelPos1(3);
    vector<vector<double> > tempArrayRelPos1;


    double new_y,old_y = 0;
    Mat frame;
    vector<int> markerIds(2);
    vector<vector<Point2f> > markerCorners, rejectedCandidates;
    aruco::DetectorParameters parameters;
    vector<Vec3d> rotationVectors, translationVectors;
    Ptr< aruco::Dictionary> markerDictionary = aruco::getPredefinedDictionary(aruco::DICT_7X7_50);
    int loopCondition = 0;

    VideoCapture vid(0);

    if(!vid.isOpened()){
        return -1;
        cout << "no dice" << endl;
    }
    namedWindow("Webcam",CV_WINDOW_AUTOSIZE);

    while(loopCondition == 0){

        if(!vid.read(frame))
            break;

        aruco::detectMarkers(frame, markerDictionary, markerCorners, markerIds);
        aruco::estimatePoseSingleMarkers(markerCorners, arucoSquareDimension, cameraMatrix, distanceCoefficients, rotationVectors, translationVectors);

        int max = markerIds.size();
        int basePos = findIndex(markerIds, baseMarker);
        int Pos1 = findIndex(markerIds, toFindMarker);

        for(int i = 0; i < max; i++){
            aruco::drawAxis(frame, cameraMatrix, distanceCoefficients, rotationVectors[i], translationVectors[i], 0.08f);
        }
        imshow("Webcam", frame);

        markerIds.resize(2); markerIds[0] = -1;  markerIds[1] = -1;

        char character = waitKey(1000/fps);
        switch(character)
        {
            case' ': //space_bar
                if(Pos1 != -1 && basePos != -1){
                    /* what follows is a cheap optimization, a larger Y is usually correct */
                    for(int i = 0; i<5; i++){
                        if(!vid.read(frame))
                            break;
                        aruco::detectMarkers(frame, markerDictionary, markerCorners, markerIds);
                        aruco::estimatePoseSingleMarkers(markerCorners, arucoSquareDimension, cameraMatrix, distanceCoefficients, rotationVectors, translationVectors);

                        int max = markerIds.size();
                        int basePos = findIndex(markerIds, baseMarker);
                        int Pos1 = findIndex(markerIds, toFindMarker);

                        for(int i = 0; i < max; i++){
                            aruco::drawAxis(frame, cameraMatrix, distanceCoefficients, rotationVectors[i], translationVectors[i], 0.08f);
                        }
                        imshow("Webcam", frame);

                        findRelativeVectors(basePos, Pos1, translationVectors, rotationVectors, relPos1, relRot1);
                        new_y = relPos1[1];
                        if(new_y > old_y){
                            relPos1[1] = new_y;
                            old_y = new_y;
                        }
                        else if(new_y < old_y){
                            relPos1[1] = old_y;
                        }
                        cout << new_y*100 << endl;
                        markerIds.resize(2); markerIds[0] = -1;  markerIds[1] = -1;
                        waitKey(1000/fps);

                    }
                    loopCondition = 1;

                }
                break;
            case 13: //enter key
                break;
            case 27: //escape key
            /* relativeMatrix is used as the rotation matrix between the baseMarker and the toFindMarker */
            Mat relativeMatrix = Mat::eye(3,3, CV_64F);
            Rodrigues(rotationVectors[Pos1],relativeMatrix);
            printf("\n%lf %lf %lf", relativeMatrix.at<double>(0,0), relativeMatrix.at<double>(0,1), relativeMatrix.at<double>(0,2));
            printf("\n%lf %lf %lf", relativeMatrix.at<double>(1,0), relativeMatrix.at<double>(1,1), relativeMatrix.at<double>(1,2));
            printf("\n%lf %lf %lf", relativeMatrix.at<double>(2,0), relativeMatrix.at<double>(2,1), relativeMatrix.at<double>(2,2));
            //printf("%lf %lf %lf \n ", rotationVectors[0][0]*radtodeg, rotationVectors[0][1]*radtodeg, rotationVectors[0][2]*radtodeg);
            //waitKey();
            loopCondition = 1;
            return -1;
            break;
        }
    }
    return 1;
}

bool cam::getMatrixFromFile(string name, Mat cameraMatrix, Mat distanceCoefficients){
    ifstream inStream(name);
    if(inStream){

        uint16_t rows = cameraMatrix.rows;
        uint16_t columns = cameraMatrix.cols;

        for(int r = 0; r < rows; r++){
            for(int c = 0; c < columns; c++){
                inStream >> cameraMatrix.at<double>(r,c);
            }
        }

        rows = distanceCoefficients.rows;
        columns = distanceCoefficients.cols;


        for(int r = 0; r < rows; r++){
            for(int c = 0; c < columns; c++){
                inStream >> distanceCoefficients.at<double>(r,c);
            }
        }

        inStream.close();
        return true;
    }
    return false;
}

