#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vrlib/shader.h>
#include <vrlib/Point.h>

#include <iomanip>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <iterator>

Point::Point(Shader shader, int xdim, int ydim, int zdim, float xspacing, float yspacing, float zspacing)
:shader(shader){
  setDimension(xdim, ydim, zdim);
  setSpacing(xspacing, yspacing, zspacing);
  initPointBuffers();
}

void Point::setDimension(int xdim, int ydim, int zdim) {
  dims = { xdim, ydim, zdim };
}

void Point::setSpacing(float xspacing, float yspacing, float zspacing) {
  spacings = { xspacing, yspacing, zspacing };
}

void Point::initPointBuffers() {
  int xdim = dims[0];
  int ydim = dims[1];
  int zdim = dims[2];
  float xspacing = spacings[0];
  float yspacing = spacings[1];
  float zspacing = spacings[2];

  const int xCount = static_cast<int>(std::floor(xdim / xspacing));
  const int yCount = static_cast<int>(std::floor(ydim / yspacing));
  const int zCount = static_cast<int>(std::floor(zdim / zspacing));

  const std::array<int, 3> vtxCounts = { xCount, yCount, zCount };
  const int maxCount = *std::max_element(vtxCounts.begin(), vtxCounts.end());

  // // compute overall dimension length in 0-1 range
  const float xLen = (float)xCount / (float)maxCount;
  const float yLen = (float)yCount / (float)maxCount;
  const float zLen = (float)zCount / (float)maxCount;


  float vertices[] = {
    // positions            // texture coords
    0.0f, 0.0f, 0.0f,
    xLen, 0.0f, 0.0f,
    xLen, yLen, 0.0f,
    0.0f, yLen, 0.0f,
    0.0f, 0.0f, zLen,
    xLen, 0.0f, zLen,
    xLen, yLen, zLen,
    0.0f, yLen, zLen,
  };

  // move the local origin to center
  for (int i = 0; i < 8; ++i) {
    vertices[i*3] -= xLen/2;
    vertices[i*3 + 1] -= yLen/2;
    vertices[i*3 + 2] -= zLen/2;
  }

  float texCoords[] = {
    0.0f, 0.0f, 0.0f, // bot 4: bot left
    1.0f, 0.0f, 0.0f, // bot 4: bot right
    1.0f, 1.0f, 0.0f, // bot 4: top right
    0.0f, 1.0f, 0.0f, // bot 4: top left
    0.0f, 0.0f, 1.0f, // top 4: bot left
    1.0f, 0.0f, 1.0f, // top 4: bot right
    1.0f, 1.0f, 1.0f, // top 4: top right
    0.0f, 1.0f, 1.0f, // top 4: top left
  };

  unsigned int indices[] = {
    1, 0, 2,    0, 2, 3, // bot
    5, 4, 6,    4, 6, 7, // top
    1, 0, 5,    0, 5, 4, // front
    2, 3, 6,    3, 6, 7, // back
    0, 3, 4,    3, 4, 7, // left
    1, 2, 5,    2, 5, 6, //right
  };


  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &tbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, tbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);
  // texture coord attribute
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  // disable the 2d texCoord attribute, since it's not used. GLSL will make the value 0.
  glDisableVertexAttribArray(1);

}

void Point::drawPoint() {
  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}