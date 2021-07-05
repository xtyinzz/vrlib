#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vrlib/shader.h>
#include <vrlib/Wireframe.h>

#include <iomanip>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <iterator>

Wireframe::Wireframe(Shader shader, int xdim, int ydim, int zdim, float xspacing, float yspacing, float zspacing, bool interior /* = false*/)
  :shader(shader), interior(interior)
{
  setDimension(xdim, ydim, zdim);
  setSpacing(xspacing, yspacing, zspacing);
  initWireframeBuffers();
}

void Wireframe::setDimension(int xdim, int ydim, int zdim) {
  dims = { xdim, ydim, zdim };
}

void Wireframe::setSpacing(float xspacing, float yspacing, float zspacing) {
  spacings = { xspacing, yspacing, zspacing };
}


// Potential improvenment:
//   can standardize vertice creation as getLineVtx(xStepLen, numVtx, y, z);
//   Then, top/bot face vertices = 3 calls eachs, mid face vertices = 2 calls each

// get vtx coords on one honrizontal line
// return vector size is 3*xCount
std::vector<float> Wireframe::getLineVtx(float xStepLen, int xCount, float y, float z)
{
  std::vector<float> vertices(3 * xCount);
  int vtxIdx = 0;

  for (int i = 0; i < xCount; ++i)
  {
    vertices[vtxIdx] = i * xStepLen;
    vertices[vtxIdx + 1] = y;
    vertices[vtxIdx + 2] = z;
    vtxIdx += 3;
  }
  return vertices;
}


// get vtx coords for a face without internal vtx.
std::vector<float> Wireframe::getFaceVtxNoInterior(int xCount, int yCount, float xStepLen, float yStepLen, float z)
{
  //NOTE: front is equivalent to bottom , back equivalent to top, in terms of a face/square.

  std::vector<float> faceVtx;
  // get vtx coords on the front line
  std::vector<float> frontLineVtx = getLineVtx(xStepLen, xCount, 0, z);
  // get vtx coords on each of the middle parts left to right, front to back/bot to top.
  std::vector<float> midLineVtx;
  midLineVtx.reserve(6 * (yCount - 2)); // reserve memory for (yCount - 2) # of lines, 6 float per line, to speed up
  for (int j = 1; j < yCount-1; ++j) {
    const float xdimLen = xStepLen*(xCount-1);
    std::vector<float> tmpMidLineVtx = getLineVtx(xdimLen, 2, j*yStepLen, z);
    //std::cout << tmpMidLineVtx.size() << '\n';
    midLineVtx.insert(midLineVtx.end(), tmpMidLineVtx.begin(), tmpMidLineVtx.end());
  }
  // get vtx coords on the back line
  std::vector<float> backLineVtx = getLineVtx(xStepLen, xCount, (yCount-1)*yStepLen, z);

  // concatenate all vtx coords: front, mid, back.
  //std::cout << faceVtx.size() << " :faceVtx size BEFORE concatenation\n";
  faceVtx.reserve(frontLineVtx.size() + midLineVtx.size() + backLineVtx.size());
  faceVtx.insert(faceVtx.end(), frontLineVtx.begin(), frontLineVtx.end());
  faceVtx.insert(faceVtx.end(), midLineVtx.begin(), midLineVtx.end());
  faceVtx.insert(faceVtx.end(), backLineVtx.begin(), backLineVtx.end());
  //std::cout << faceVtx.size() << " :faceVtx size AFTER concatenation\n";

  return faceVtx;
}

// get 4 vtx coords of a square
// could have implemented by calling getLineVtx
// This one is faster. R.I.P. to code reusability.
std::vector<float> Wireframe::getSquareVtx(float xdimLen, float ydimLen, float z)
{
  std::vector<float> vertices = {
    0, 0, z,
    xdimLen, 0, z,
    0, ydimLen, z,
    xdimLen, ydimLen, z
  };

  return vertices;
}

// get vtx idx to draw lines in one direction on one face, excluding both ends.
// baseIdx: idx of the first internal vtx on the starting edge dimension
// edgeIdxStep: index increment from starting vtx to ending vtx of a line (internal vtx)
std::vector<int> Wireframe::getInternalWireIndicesOneDir(int dimCount, int baseIdx, float internalStep, float endStep)
{
  const int lineCount = dimCount - 2; // excluding two ends, so there two 2 fewer lines
  std::vector<int> indices(2*lineCount);
  for (int i = 0; i < lineCount; ++i) {
    indices[i*2] = baseIdx + i*internalStep;
    indices[i*2 + 1] = baseIdx + endStep + i*internalStep;
  }
  return indices;
}

// get vtx idx to draw lines in two opposing direction on one face, excluding ends on both dir.
std::vector<int> Wireframe::getInternalWireIndicesTwoDir(
    int dimCount1, int dimCount2,
    int baseIdx1, int baseIdx2,
    float internalStep1, float internalStep2,
    float endStep1, float endStep2)
{
  std::vector<int> lineIndicesDim1 = getInternalWireIndicesOneDir(dimCount1, baseIdx1, internalStep1, endStep1);
  std::vector<int> lineIndicesDim2 = getInternalWireIndicesOneDir(dimCount2, baseIdx2, internalStep2, endStep2);


  std::vector<int> twoDirIndices;
  twoDirIndices.reserve(lineIndicesDim1.size() + lineIndicesDim2.size());
  twoDirIndices.insert(twoDirIndices.end(), lineIndicesDim1.begin(), lineIndicesDim1.end());
  twoDirIndices.insert(twoDirIndices.end(), lineIndicesDim2.begin(), lineIndicesDim2.end());

  // Print indices DEBUG
  // std::cout << "Two Direction Indices: \n";
  // for (int i = 0; i < twoDirIndices.size(); ++i) {
  //   std::cout << twoDirIndices[i] << " ";
  //   if (i % 2 == 1) std::cout << "\n";
  // }

  return twoDirIndices;
}

std::vector<int> Wireframe::getWireframIndices(std::array<int, 3> vtxCounts)
{
  const int numTopBotVtx = vtxCounts[0]*vtxCounts[1] - (vtxCounts[0]-2)*(vtxCounts[1]-2);

  // init indices
  const int xInternalStep = 1;
  const int yInternalStep = 2;
  const int zInternalStep = 4;
  // # of idx inclement for a line crossing a dimension (i.e. the changing dimension)
  // e.g. zEndStep: # of inclement when the ending vtx crosses z dimension (z changes)
  const int zEndStep = vtxCounts[0] * 2 + 2 * (vtxCounts[1] - 2) + 4*(vtxCounts[2]-2);
  const int xEndStep = 1;
  // when the vtx crossing Y is on Top/Bot face or Mid. Different cases.
  const int yEndStepBotTop = vtxCounts[0] + 2 * (vtxCounts[1] - 2);
  const int yEndStepMid = 2;

  // draw vet and hon wire top/bot
  // starting with a base vtx index (left bot vtx) and then build up indices
  int baseIdx1, baseIdx2;
  // top/bot: dim1=X, dim2=Y
  baseIdx1 = 1;
  baseIdx2 = vtxCounts[0];
  std::vector<int> botFaceIndices = getInternalWireIndicesTwoDir(
    vtxCounts[0], vtxCounts[1],
    baseIdx1, baseIdx2,
    xInternalStep, yInternalStep,
    yEndStepBotTop, xEndStep
  );
  baseIdx1 += zEndStep;
  baseIdx2 += zEndStep;
  std::vector<int> topFaceIndices = getInternalWireIndicesTwoDir(
    vtxCounts[0], vtxCounts[1],
    baseIdx1, baseIdx2,
    xInternalStep, yInternalStep,
    yEndStepBotTop, xEndStep
  );

  // draw hon wire side
  // left/right: dim1=Y, dim2=Z
  baseIdx1 = vtxCounts[0];
  baseIdx2 = numTopBotVtx;
  std::vector<int> leftFaceIndices = getInternalWireIndicesTwoDir(
    vtxCounts[1], vtxCounts[2],
    baseIdx1, baseIdx2,
    yInternalStep, zInternalStep,
    zEndStep, yEndStepMid
  );
  baseIdx1 += xEndStep;
  baseIdx2 += xEndStep;
  std::vector<int> rightFaceIndices = getInternalWireIndicesTwoDir(
    vtxCounts[1], vtxCounts[2],
    baseIdx1, baseIdx2,
    yInternalStep, zInternalStep,
    zEndStep, yEndStepMid
  );
  
  // draw ver wire side
  // front/back: dim1=X, dim2=Z
  baseIdx1 = 1;
  baseIdx2 = numTopBotVtx;
  std::vector<int> frontFaceIndices = getInternalWireIndicesTwoDir(
    vtxCounts[0], vtxCounts[2],
    baseIdx1, baseIdx2,
    xInternalStep, zInternalStep,
    zEndStep, xEndStep
  );
  baseIdx1 += yEndStepBotTop;
  baseIdx2 += yEndStepMid;
  std::vector<int> backFaceIndices = getInternalWireIndicesTwoDir(
    vtxCounts[0], vtxCounts[2],
    baseIdx1, baseIdx2,
    xInternalStep, zInternalStep,
    zEndStep, xEndStep
  );

  std::vector<int> indices;
  indices.reserve(
    topFaceIndices.size() + botFaceIndices.size() + 
    leftFaceIndices.size() + rightFaceIndices.size() +
    frontFaceIndices.size() + backFaceIndices.size()
  );

  indices.insert(indices.end(), topFaceIndices.begin(), topFaceIndices.end());
  indices.insert(indices.end(), botFaceIndices.begin(), botFaceIndices.end());
  indices.insert(indices.end(), leftFaceIndices.begin(), leftFaceIndices.end());
  indices.insert(indices.end(), rightFaceIndices.begin(), rightFaceIndices.end());
  indices.insert(indices.end(), frontFaceIndices.begin(), frontFaceIndices.end());
  indices.insert(indices.end(), backFaceIndices.begin(), backFaceIndices.end());

  return indices;
}

// get vtx idx to draw the volume outline, with vtx indices setup of Wireframe (NO interior).
std::vector<int> Wireframe::getOutlineIndices(std::array<int, 3> vtxCounts)
{
  const int zEndStep = vtxCounts[0] * 2 + 2 * (vtxCounts[1] - 2) + 4*(vtxCounts[2]-2);
  // when the vtx crossing Y is on Top/Bot face or Mid. Different cases.
  const int yEndStepBotTop = vtxCounts[0] + 2 * (vtxCounts[1] - 2);
  const std::vector<int> botVtxIdx =  { 0, vtxCounts[0]-1, vtxCounts[0]-1 + yEndStepBotTop, yEndStepBotTop };
  std::vector<int> botSquareIdx(8);
  std::vector<int> topSquareIdx(8);
  std::vector<int> pillarsIdx(8);

  for (int i = 0; i < botVtxIdx.size(); i++) {
    const int endVtxIdx = (i+1) % botVtxIdx.size();
    const int lineArrayIdx = i*2;
    botSquareIdx[lineArrayIdx] = botVtxIdx[i];
    botSquareIdx[lineArrayIdx+1] = botVtxIdx[endVtxIdx];
    topSquareIdx[lineArrayIdx] = botVtxIdx[i] + zEndStep;
    topSquareIdx[lineArrayIdx+1] = botVtxIdx[endVtxIdx] + zEndStep;
    pillarsIdx[lineArrayIdx] = botVtxIdx[i];
    pillarsIdx[lineArrayIdx+1] = botVtxIdx[i] + zEndStep;
  }

  std::vector<int> outlineIdx;
  outlineIdx.reserve(botSquareIdx.size() + topSquareIdx.size() + pillarsIdx.size());
  outlineIdx.insert(outlineIdx.end(), botSquareIdx.begin(), botSquareIdx.end());
  outlineIdx.insert(outlineIdx.end(), topSquareIdx.begin(), topSquareIdx.end());
  outlineIdx.insert(outlineIdx.end(), pillarsIdx.begin(), pillarsIdx.end());

  return outlineIdx;
}

void Wireframe::initWireframeBuffers() {
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
  // const dimLength = vtxCounts.map(e => e / maxCount);
  const std::array<float, 3> dimLength = {
    (float)xCount / (float)maxCount,
    (float)yCount / (float)maxCount,
    (float)zCount / (float)maxCount
  };

  // // get the length between vertices along each dim
  // const [xStepLen, yStepLen, zStepLen] = dimLength.map((e, i) => e / (vtxCounts[i] - 1));
  const float xStepLen = dimLength[0] / (xCount - 1);
  const float yStepLen = dimLength[1] / (yCount - 1);
  const float zStepLen = dimLength[2] / (zCount - 1);

  // // init vertices
  std::vector<float> botFaceVertices = getFaceVtxNoInterior(
    xCount, 
    yCount,
    xStepLen,
    yStepLen,
    0
  );

  std::vector<float> topFaceVertices = getFaceVtxNoInterior(
    xCount,
    yCount,
    xStepLen,
    yStepLen,
    dimLength[2]
  );

  std::vector<float> midFaceVertices;
  midFaceVertices.reserve(12 * (zCount - 2)); // preallocate mem for 3 floats per vtx, 4 vtx per mid-face, zCount - 2 # of mid-faces.
  for (int k = 1; k < zCount-1; ++k) {
    std::vector<float> tmpMidFaceVertices = getSquareVtx(dimLength[0], dimLength[1], k*zStepLen);
    midFaceVertices.insert(midFaceVertices.end(), tmpMidFaceVertices.begin(), tmpMidFaceVertices.end());
  }

  std::vector<float> vertices;
  int verticesNum = botFaceVertices.size() + midFaceVertices.size() + topFaceVertices.size();
  vertices.reserve(verticesNum);
  vertices.insert(vertices.end(), botFaceVertices.begin(), botFaceVertices.end());
  vertices.insert(vertices.end(), midFaceVertices.begin(), midFaceVertices.end());
  vertices.insert(vertices.end(), topFaceVertices.begin(), topFaceVertices.end());

  for (int i = 0; i < verticesNum/3; ++i) {
    vertices[i*3] -= dimLength[0] / 2;
    vertices[i*3 + 1] -= dimLength[1] / 2;
    vertices[i*3 + 2] -= dimLength[2] / 2;
  }
  //std::cout << "start to print vtx coords\n";
  // for (int i = 0; i < vertices.size(); ++i) {
  //   std::cout << std::setw(4) << vertices[i] << " ";
  //   if (i % 3 == 2 && i != 0) std::cout << "\n";
  // }
  //std::cout << "\nEND coord printing\n";

  // init indices
  std::vector<int> internalWireframeIdx = getWireframIndices(vtxCounts);
  std::vector<int> outlineIdx = getOutlineIndices(vtxCounts);
  std::vector<int> indices;
  indices.reserve(internalWireframeIdx.size() + outlineIdx.size());
  indices.insert(indices.end(), internalWireframeIdx.begin(), internalWireframeIdx.end());
  indices.insert(indices.end(), outlineIdx.begin(), outlineIdx.end());

  std::vector<GLuint> indicesGL(indices.begin(), indices.end());
  std::vector<GLuint> outlineIdxGL(outlineIdx.begin(), outlineIdx.end());

  // set the idx counts for drawElement calls
  wireframeIdxCount = indices.size();
  outlineIdxCount = outlineIdx.size();

  // std::cout << "Wireframe Indices:\n";
  // for (int i = 0; i < indices.size(); ++i) {
  //   std::cout << std::setw(2) << indices[i] << "\n";
  //   if (i % 2 == 1 && i != 0) std::cout << "\n";
  // }

  // std::cout << "Outline Indices and coords:\n";
  // for (int i = 0; i < outlineIdx.size(); ++i) {
  //   std::cout << std::setw(4) << outlineIdx[i] << " \n";
  //   std::cout << std::setw(4) << vertices[outlineIdx[i]*3] << ' ' << vertices[outlineIdx[i]*3+1] << ' ' << vertices[outlineIdx[i]*3+2] << " \n";
  //   if (i % 2 == 1 && i != 0) std::cout << "\n";
  // }

  glGenBuffers(1, &vbo);
  glGenBuffers(1, &eboWireframe);
  glGenBuffers(1, &eboOutline);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboWireframe);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesGL.size() * sizeof(indicesGL[0]), indicesGL.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboOutline);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, outlineIdxGL.size() * sizeof(outlineIdxGL[0]), outlineIdxGL.data(), GL_STATIC_DRAW);

  // compare sizeof
  // std::cout << sizeof(outlineIdx) << '\n' << outlineIdx.size() * sizeof(outlineIdx[0]) << '\n';
  // std::cout << sizeof(outlineIdxGL) << '\n' << outlineIdxGL.size() * sizeof(outlineIdxGL[0]) << '\n';
  // std::cout << "vertices sizeof: " << sizeof(vertices) << " - " << sizeof(vertices[0]) << '\n';
  // convert indices from int to unsigned int

  // bind VAO for Wireframe (outline + internal)
  glGenVertexArrays(1, &vaoWireframe);
  glBindVertexArray(vaoWireframe);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
  //glBindBuffer(GL_ARRAY_BUFFER, 0); 
  //indices
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboWireframe);
  // remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
  // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
  // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
  glBindVertexArray(0); 

  // bind VAO for outline
  glGenVertexArrays(1, &vaoOutline);
  glBindVertexArray(vaoOutline);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
  //glBindBuffer(GL_ARRAY_BUFFER, 0); 
  //indices
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboOutline);
  // remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
  // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
  // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
  glBindVertexArray(0); 
}

void Wireframe::drawWireframe()
{
  // render the triangle
  glBindVertexArray(vaoWireframe);
  glDrawElements(GL_LINES, wireframeIdxCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void Wireframe::drawOutline()
{
  // render the triangle
  glBindVertexArray(vaoOutline);
  glDrawElements(GL_LINES, outlineIdxCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
