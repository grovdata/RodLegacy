#include "cardata.h"
#include "utils.h"

CarData::CarData() {
    CarModel chev2dr;
    chev2dr.id = CAR_MODEL_GM_CHEVROLET_2DR;
    chev2dr.vendor = GM;
    chev2dr.name = "Chevrolet 2-dr coupe";
    chev2dr.filename = "../data/res/car1.gif";
    chev2dr.year = 1940;
    chev2dr.baseSpeed = 98;
    chev2dr.width = 232;
    chev2dr.height = 69;
    chev2dr.rearWheelX = 30;
    chev2dr.rearWheelY = 38;
    chev2dr.gasCapX = 75;
    chev2dr.gasCapY = 36;
    chev2dr.frontWheelX = 178;
    chev2dr.frontWheelY = 39;
    chev2dr.driverX = 106;
    chev2dr.driverY = 7;

    printf("woop %s \n", chev2dr.filename.c_str());
    allCarModels.insert(chev2dr);

    CarModel saab92;
    chev2dr.id = CAR_MODEL_MOD_SAAB_92;
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
    allCarModels.insert(saab92);

}

CarData::~CarData() {

}

CarModel CarData::GetModel(CarModelId carModel) {
    for (auto m: allCarModels) {
        printf("Model %s\n", m.name.c_str());
        if (m.id == carModel) {
            printf("found model %d\n",m.id);
            return m;
        }
    }
    // Not sure about this
    return CarModel();
}

void CarData::PrintModel(CarModel model) {
    printf("Car ID: %d\n", model.id);
    printf("Vendor: %d\n", model.vendor);
    printf("Name: %s\n", model.name.c_str());
    printf("Filename: %s\n", model.filename.c_str());
    printf("Year: %d\n", model.year);
    printf("Base Speed: %d\n", model.baseSpeed);
    printf("Width: %d, Height: %d\n", model.width, model.height);
    printf("Driver Position: (%d, %d)\n", model.driverX, model.driverY);
    printf("Rear Wheel Position: (%d, %d)\n", model.rearWheelX, model.rearWheelY);
    printf("Front Wheel Position: (%d, %d)\n", model.frontWheelX, model.frontWheelY);
    printf("Gas Cap Position: (%d, %d)\n", model.gasCapX, model.gasCapY);
};

CarModel CarData::GetRandomModel() {
    return getRandomElement(allCarModels);
}