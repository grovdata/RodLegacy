#include "cardata.h"

CarData::CarData() {
    CarModel chev2dr;
    chev2dr.vendor = GM;
    chev2dr.name = "Chevrolet 2-dr coupe";
    chev2dr.filename = "../data/res/car1.gif";
    chev2dr.year = 1940;
    chev2dr.baseSpeed = 98;
    chev2dr.width = 232;
    chev2dr.height = 69;
    allCarModels.push_back(chev2dr);

    CarModel saab92;
    saab92.vendor = GM;
    saab92.name = "Saab 92";
    saab92.filename = "../data/mods/saab92.png";
    saab92.year = 1948;
    saab92.baseSpeed = 98;
    saab92.width = 140;
    saab92.height = 51;
    saab92.rearWheelX = 15;
    saab92.rearWheelY = 23;
    saab92.frontWheelX = 102;
    saab92.frontWheelY = 23;
    saab92.driverX = 66;
    saab92.driverY = 2;
    allCarModels.push_back(saab92);

}

CarData::~CarData() {

}