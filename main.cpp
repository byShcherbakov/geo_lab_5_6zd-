// Практическая работа 5. Вариант 2. Солнечная система.
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <vector>

constexpr float pi = 3.14159265359f;
struct Vertex { float x,y,z,nx,ny,nz; };
struct Mesh { GLuint vao=0,vbo=0,ebo=0; GLsizei count=0; };

class Scene : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
public:
    Scene() {
        resize(900,800);
        setWindowTitle(QString::fromUtf8("Практическая работа 5. Вариант 2"));
        setFocusPolicy(Qt::StrongFocus);
        clock.start();
        connect(&timer,&QTimer::timeout,this,[this] {
            float dt=std::min(clock.restart()/1000.0f,0.1f);
            if (animate) { time+=dt; update(); }
        });
        timer.start(16);
    }
    ~Scene() override { cleanup(); }

protected:
    void initializeGL() override {
        if (!initializeOpenGLFunctions()) qFatal("OpenGL 3.3 Core is required");
        connect(context(),&QOpenGLContext::aboutToBeDestroyed,this,[this]{cleanup();},Qt::DirectConnection);
        const char *vs=R"(
            #version 330 core
            layout(location=0) in vec3 position;
            layout(location=1) in vec3 normal;
            uniform mat4 M,V,P;
            out vec3 N;
            void main() {
                N=mat3(transpose(inverse(M)))*normal;
                gl_Position=P*V*M*vec4(position,1.0);
            })";
        const char *fs=R"(
            #version 330 core
            in vec3 N;
            uniform vec3 color;
            uniform bool plain;
            out vec4 frag;
            void main() {
                float shade=plain ? 1.0 : 0.4+0.6*max(dot(normalize(N),normalize(vec3(1,2,3))),0.0);
                frag=vec4(color*shade,1.0);
            })";
        if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex,vs) ||
            !program.addShaderFromSourceCode(QOpenGLShader::Fragment,fs) || !program.link())
            qFatal("Shader error: %s",qPrintable(program.log()));

        // Единичная сфера из треугольников. Радиусы задаются матрицей M.
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        const int stacks=24,slices=48;
        for (int i=0;i<=stacks;++i) {
            float a=pi*i/stacks;
            for (int j=0;j<=slices;++j) {
                float b=2*pi*j/slices;
                float x=std::sin(a)*std::cos(b), y=std::cos(a), z=std::sin(a)*std::sin(b);
                vertices.push_back({x,y,z,x,y,z});
            }
        }
        for (int i=0;i<stacks;++i) for (int j=0;j<slices;++j) {
            GLuint a=i*(slices+1)+j,b=a+slices+1;
            indices.insert(indices.end(),{a,b,a+1,a+1,b,b+1});
        }
        createMesh(sphere,vertices,indices);
        vertices.clear(); indices.clear();
        for (int j=0;j<160;++j) {
            float a=2*pi*j/160;
            vertices.push_back({std::cos(a),0,std::sin(a),0,1,0});
            indices.push_back(j);
        }
        createMesh(orbit,vertices,indices);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }

    void paintGL() override {
        glClearColor(0.025f,0.03f,0.055f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glViewport(0,0,qRound(width()*devicePixelRatioF()),qRound(height()*devicePixelRatioF()));
        QMatrix4x4 projection,view,root;
        // Параметры варианта 2: FOV=40, near=5, far=10.
        projection.perspective(40.0f,float(width())/std::max(height(),1),5.0f,10.0f);
        view.lookAt(camera(),QVector3D(0,0,0),QVector3D(0,1,0));
        root.translate(offset);
        root.rotate(objectAngle,0,1,0);
        program.bind();
        program.setUniformValue("V",view);
        program.setUniformValue("P",projection);
        draw(sphere,root,{},0.43f,{1.0f,0.68f,0.12f},true);
        const float radius[]={.62f,.86f,1.12f,1.37f,1.68f,1.97f,2.22f,2.46f};
        const float size[]={.055f,.075f,.08f,.065f,.15f,.125f,.095f,.09f};
        const float phase[]={0.2f,2.1f,4.0f,5.6f,1.0f,3.3f,5.0f,2.5f};
        const QVector3D colors[]={{.65f,.6f,.55f},{.95f,.7f,.3f},{.16f,.5f,1},
            {.9f,.25f,.12f},{.85f,.61f,.4f},{.9f,.8f,.5f},{.4f,.85f,.9f},{.2f,.3f,.85f}};
        for (int i=0;i<8;++i) {
            QMatrix4x4 ring=root;
            ring.scale(radius[i]);
            program.setUniformValue("M",ring);
            program.setUniformValue("color",QVector3D(.24f,.27f,.34f));
            program.setUniformValue("plain",true);
            glBindVertexArray(orbit.vao);
            glDrawElements(GL_LINE_LOOP,orbit.count,GL_UNSIGNED_INT,nullptr);
            float a=phase[i]+time*(.55f/(1+i*.65f));
            QVector3D position(radius[i]*std::cos(a),0,radius[i]*std::sin(a));
            draw(sphere,root,position,size[i],colors[i],false);
            if (i==5) { // кольца Сатурна
                QMatrix4x4 saturn=root;
                saturn.translate(position); saturn.rotate(25,1,0,0);
                for (float r : {.17f,.19f,.21f}) {
                    QMatrix4x4 m=saturn; m.scale(r);
                    program.setUniformValue("M",m);
                    program.setUniformValue("color",QVector3D(.75f,.65f,.42f));
                    program.setUniformValue("plain",true);
                    glBindVertexArray(orbit.vao);
                    glDrawElements(GL_LINE_LOOP,orbit.count,GL_UNSIGNED_INT,nullptr);
                }
            }
        }
        glBindVertexArray(0); program.release();
    }

    void keyPressEvent(QKeyEvent *event) override {
        switch (event->key()) {
        case Qt::Key_A: azimuth-=5; break;
        case Qt::Key_D: azimuth+=5; break;
        case Qt::Key_W: distance=std::max(6.5f,distance-.15f); break;
        case Qt::Key_S: distance=std::min(8.5f,distance+.15f); break;
        case Qt::Key_Left: offset-=cameraRight()*.15f; break;
        case Qt::Key_Right: offset+=cameraRight()*.15f; break;
        case Qt::Key_Q: objectAngle-=5; break;
        case Qt::Key_E: objectAngle+=5; break;
        case Qt::Key_Space: animate=!animate; break;
        case Qt::Key_R: azimuth=0; distance=7.5f; offset={}; objectAngle=0; time=0; animate=false; break;
        case Qt::Key_Escape: close(); return;
        default: QOpenGLWidget::keyPressEvent(event); return;
        }
        update();
    }

private:
    QOpenGLShaderProgram program;
    Mesh sphere,orbit;
    QTimer timer;
    QElapsedTimer clock;
    float azimuth=0,distance=7.5f,objectAngle=0,time=0;
    QVector3D offset;
    bool animate=false;
    QVector3D camera() const {
        float a=azimuth*pi/180, elevation=35*pi/180;
        return distance*QVector3D(std::sin(a)*std::cos(elevation),std::sin(elevation),std::cos(a)*std::cos(elevation));
    }
    QVector3D cameraRight() const {
        return QVector3D::crossProduct(-camera().normalized(),QVector3D(0,1,0)).normalized();
    }
    void createMesh(Mesh &mesh,const std::vector<Vertex>& v,const std::vector<GLuint>& index) {
        mesh.count=static_cast<GLsizei>(index.size());
        glGenVertexArrays(1,&mesh.vao); glGenBuffers(1,&mesh.vbo); glGenBuffers(1,&mesh.ebo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(Vertex),v.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,index.size()*sizeof(GLuint),index.data(),GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(3*sizeof(float))); glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    void draw(const Mesh &mesh,const QMatrix4x4 &root,QVector3D position,float radius,QVector3D color,bool plain) {
        QMatrix4x4 m=root; m.translate(position); m.scale(radius);
        program.setUniformValue("M",m); program.setUniformValue("color",color); program.setUniformValue("plain",plain);
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES,mesh.count,GL_UNSIGNED_INT,nullptr);
    }
    void cleanup() {
        if (!context() || !sphere.vao) return;
        makeCurrent();
        for (Mesh *m : {&sphere,&orbit}) {
            glDeleteBuffers(1,&m->vbo); glDeleteBuffers(1,&m->ebo); glDeleteVertexArrays(1,&m->vao); *m={};
        }
        program.removeAllShaders(); doneCurrent();
    }
};

int main(int argc,char *argv[]) {
    QSurfaceFormat f; f.setVersion(3,3); f.setProfile(QSurfaceFormat::CoreProfile); f.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc,argv); Scene window; window.show(); return app.exec();
}
