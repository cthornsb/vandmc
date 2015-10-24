#ifndef CAMERA_H
#define CAMERA_H

#include <QMainWindow>
#include <QGraphicsScene>

#include "vikar_core.h"

class Primitive;

namespace Ui {
class Camera;
}

class Camera : public QMainWindow
{
    Q_OBJECT

public:
    Camera(QWidget* parent=0);

    Camera(double x_, double y_, double z_, double theta_=0, double phi_=0, QWidget* parent=0);

    virtual ~Camera();

    Vector3 &GetPosition(){ return pos; }

    Vector3 &GetDirection(){ return dir; }

    Matrix3 &GetRotation(){ return rot; }

    double GetFOV(){ return fov; }

    int GetScaling(){ return scaling; }

    int GetSizeX(){ return sizeX; }

    int GetSizeY(){ return sizeY; }

    std::vector<Primitive*> *GetPrimitives(){ return &primitives; }

    void SetPosition(double x_, double y_, double z_);

    void SetRotation(double theta_, double phi_, double psi_);

    void PointCamera(const Vector3 &g_);

    void SetFOV(double fov_);

    void SetScaling(int scaling_);

    void SetSizeX(int sizeX_);

    void SetSizeY(int sizeY_);

    void SetApp(QApplication *the_app_){ the_app = the_app_; }

    void AddPrimitive(Primitive *prim_);

    virtual void Render();

    virtual void paintEvent(QPaintEvent*);

private slots:

    void on_pushButton_clicked();

    void on_doubleSpinBox_valueChanged(double arg1);

    void on_doubleSpinBox_2_valueChanged(double arg1);

    void on_doubleSpinBox_3_valueChanged(double arg1);

    void on_doubleSpinBox_5_valueChanged(double arg1);

    void on_doubleSpinBox_6_valueChanged(double arg1);

    void on_doubleSpinBox_7_valueChanged(double arg1);

    void on_spinBox_valueChanged(int arg1);

    void on_spinBox_2_valueChanged(int arg1);

    void on_actionScreenshot_triggered();

    void on_pushButton_2_clicked();

    void on_actionExit_triggered();

private:
    Ui::Camera *ui;
    QGraphicsScene *scene;
    QApplication *the_app;
    QPixmap *pixmap;

    Vector3 pos; /// The cartesian position of the camera.
    Vector3 dir; /// The cartesian direction of the camera.
    Vector3 screenX;
    Vector3 screenY;

    Matrix3 rot; /// The 3d rotation matrix for the camera.

    double fov; /// The field of view of the camera (rad).
    int scaling;

    int sizeX; /// The x-axis size of the screen in pixels.
    int sizeY; /// The y-axis size of the screen in pixels.

    Vector3 center; /// The center of the screen.
    Vector3 origin; /// The top-left of the screen.

    double pixelX; /// Size of a pixel along the x-axis (m).
    double pixelY; /// Size of a pixel along the y-axis (m).

    double theta;
    double phi;
    double psi;
    bool rotated;

    std::vector<Primitive*> primitives;

    void set_pixel_size();

    void set_rotation();
};

#endif // CAMERA_H
