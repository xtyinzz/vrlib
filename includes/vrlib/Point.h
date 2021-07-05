#ifndef POINT_H
#define POINT_H

#include <vrlib/shader.h>
#include <array>

// render the cartesian grid volume as points with a colormap
class Point {
  private:
    Shader shader;
    unsigned int vao, vbo, tbo, ebo;
    std::array<int, 3> dims;
    std::array<float, 3> spacings;

  public:
    Point(Shader shader, int xdim, int ydim, int zdim, float xspacing, float yspacing, float zspacing);
    void setDimension(int xdim, int ydim, int zdim);
    void setSpacing(float xspacing, float yspacing, float zspacing);
    //void setShader(Shader shader);
    std::array<int, 3> getDimension() { return dims; }
    std::array<float, 3> getSpacing() { return spacings; }

    void initPointBuffers();
    void drawPoint();
};

#endif