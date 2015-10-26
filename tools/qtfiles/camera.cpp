#include <QPainter>
#include <QPixmap>

#include "camera.h"
#include "ui_camera.h"

#include "detectors.h"

void Camera::set_pixel_size(){
    pixelX = 2.0*std::tan(fov/(2.0*scaling));
    pixelY = 2.0*std::tan(fov/(2.0*scaling));
    center = pos + dir;
    origin = center + screenX*(-pixelX/2.0) + screenY*(-pixelY/2.0);
    pixelX = pixelX / sizeX;
    pixelY = pixelY / sizeY;
}

void Camera::set_rotation(){
    dir = Vector3(0.0, 0.0, 1.0);
    screenX = Vector3(1, 0, 0);
    screenY = Vector3(0, -1, 0);

    rot.SetRotationMatrixSphere(theta, phi, psi);
    rot.Transform(dir);
    rot.Transform(screenX);
    rot.Transform(screenY);
    rotated = false;
}

Camera::Camera(QWidget* parent): QMainWindow(parent), ui(new Ui::Camera) {
    fov = pi/2.0;
    sizeX = 240;
    sizeY = 240;
    scaling = 1;

    SetRotation(0.0, 0.0, 0.0);

    ui->setupUi(this);

    scene = new QGraphicsScene(this);
    ui->graphicsView->setScene(scene);

    pixmap = new QPixmap(sizeX, sizeY);
    pixmap->fill(Qt::black);

    scene->addPixmap(*pixmap);
    the_app = NULL;
}

Camera::Camera(double x_, double y_, double z_, double theta_, double phi_, QWidget* parent): QMainWindow(parent), ui(new Ui::Camera) {
    fov = pi/2.0;
    sizeX = 240;
    sizeY = 240;
    scaling = 1;

    pos = Vector3(x_, y_, z_);
    SetRotation(theta_, phi_, 0.0);

    ui->setupUi(this);

    scene = new QGraphicsScene(this);
    ui->graphicsView->setScene(scene);

    pixmap = new QPixmap(sizeX, sizeY);
    pixmap->fill(Qt::black);

    scene->addPixmap(*pixmap);
    the_app = NULL;
}

Camera::~Camera(){
    for(std::vector<Primitive*>::iterator iter = primitives.begin(); iter != primitives.end(); iter++){
        delete (*iter);
    }
    primitives.clear();
    delete ui;
    delete scene;
    delete pixmap;
}

void Camera::SetPosition(double x_, double y_, double z_){
    pos = Vector3(x_, y_, z_);
}

void Camera::SetRotation(double theta_, double phi_, double psi_){
    theta = theta_;
    phi = phi_;
    psi = psi_;
    rotated = true;
}

void Camera::PointCamera(const Vector3 &g_){
    dir = g_ - pos;
    dir.Normalize();
}

void Camera::SetFOV(double fov_){
    fov = fov_*deg2rad;
}

void Camera::SetScaling(int scaling_){
    scaling = scaling_;
}

void Camera::SetSizeX(int sizeX_){
    sizeX = sizeX_;
}

void Camera::SetSizeY(int sizeY_){
    sizeY = sizeY_;
}

void Camera::AddPrimitive(Primitive *prim_){
    primitives.push_back(prim_);
}

void Camera::Render() {
    if(rotated){ set_rotation(); }
    set_pixel_size();

    Vector3 p1;

    pixmap->fill(Qt::black);
    QPainter pen(pixmap);
    QColor pen_color;

    Vector3 ray;
    Vector3 normal;

    Vector3 face_hit1;
    Vector3 face_hit2;

    int rgb; // Color of the drawing pen.
    double currentX; // Current pixel along the x-axis.
    double currentY; // Current pixel along the y-axis.
    double depth; // Distance of the "pixel" from the viewer.
    double t1; // "Distance" parameter such that P1 = position + ray*t1.
    double t2; // Not used.
    for(int i = 0; i < sizeY; i++){
    	ui->progressBar->setValue((float(i)*100/sizeY));
        currentY = (pixelY/2.0) + i*pixelY;
        for(int j = 0; j < sizeX; j++){ // Loop over all rows of pixels.
            currentX = (pixelX/2.0) + j*pixelX;
            ray = origin + screenX*currentX + screenY*currentY - pos;
            ray.Normalize(); // Normalize the direction vector.
            
            // Set the initial conditions.
            depth = 9999;
            rgb = 0;
            
            // Loop over all pixels in this row.
            for(std::vector<Primitive*>::iterator iter = primitives.begin(); iter != primitives.end(); iter++){
                if((*iter)->IntersectPrimitive(pos, ray, p1, normal, t1, t2) && t1 < depth){
			        rgb = (int)(fabs(ray.CosAngle(normal))*255);
			        if(rgb < 0 || rgb > 255){ continue; }
                	depth = t1;
                }
            }
            
            // Only draw the pixel if we found a valid intersection.
            if(depth != 9999){ 
                pen_color.setRgb(rgb, rgb, rgb);
                pen.setPen(pen_color);
                pen.drawPoint(j, i);
            }
        }
    }

	ui->progressBar->setValue(100);

    scene->addPixmap(*pixmap);
}

void Camera::paintEvent(QPaintEvent*) {
}

void Camera::on_pushButton_clicked(){
    this->Render();
}

void Camera::on_doubleSpinBox_valueChanged(double arg1){
    pos.axis[0] = arg1;
}

void Camera::on_doubleSpinBox_2_valueChanged(double arg1){
    pos.axis[1] = arg1;
}

void Camera::on_doubleSpinBox_3_valueChanged(double arg1){
    pos.axis[2] = arg1;
}

void Camera::on_doubleSpinBox_5_valueChanged(double arg1){
    theta = -arg1*deg2rad;
    rotated = true;
}

void Camera::on_doubleSpinBox_6_valueChanged(double arg1){
    phi = -arg1*deg2rad;
    rotated = true;
}

void Camera::on_doubleSpinBox_7_valueChanged(double arg1){
    psi = arg1*deg2rad;
    rotated = true;
}

void Camera::on_spinBox_valueChanged(int arg1){
    fov = arg1*deg2rad;
}

void Camera::on_spinBox_2_valueChanged(int arg1){
    scaling = arg1;
}

void Camera::on_actionScreenshot_triggered(){
    pixmap->save(ui->lineEdit->text());
    std::cout << "Screenshot saved to " << ui->lineEdit->text().toStdString() << std::endl;
}

void Camera::on_pushButton_2_clicked(){
    pixmap->save(ui->lineEdit->text());
    std::cout << "Screenshot saved to " << ui->lineEdit->text().toStdString() << std::endl;
}

void Camera::on_pushButton_3_clicked(){
	for(std::vector<Primitive*>::iterator iter = primitives.begin(); iter != primitives.end(); iter++){
        delete (*iter);
    }
    primitives.clear();
    ReadDetFile(ui->lineEdit_2->text().toStdString().c_str(), primitives);
    std::cout << "Loaded detector file " << ui->lineEdit_2->text().toStdString() << std::endl;
    std::cout << " Found " << primitives.size() << " objects.\n";
}

void Camera::on_actionExit_triggered(){
    if(the_app){ the_app->quit(); }
}