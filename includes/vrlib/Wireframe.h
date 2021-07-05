#ifndef WIREFRAME_H
#define WIREFRAME_H

#include <vrlib/shader.h>
#include <vector>
#include <array>

class Wireframe {
  private:
    Shader shader;
    unsigned int vaoWireframe, vaoOutline, eboWireframe, eboOutline, vbo;
    int wireframeIdxCount, outlineIdxCount;
    std::array<int, 3> dims;
    std::array<float, 3> spacings;
    bool interior = false;

    std::vector<float> getLineVtx(float xStepLen, int xCount, float y, float z);
    std::vector<float> getFaceVtxNoInterior(int xCount, int yCount, float xStepLen, float yStepLen, float z);
    std::vector<float> getSquareVtx(float xdimLen, float ydimLen, float z);
    std::vector<int> getInternalWireIndicesOneDir(int dimCount, int baseIdx, float internalStep, float endStep);
    std::vector<int> getInternalWireIndicesTwoDir(
        int dimCount1, int dimCount2,
        int baseIdx1, int baseIdx2,
        float internalStep1, float internalStep2,
        float endStep1, float endStep2
    );
    std::vector<int> getWireframIndices(std::array<int, 3> vtxCounts);
    std::vector<int> getOutlineIndices(std::array<int, 3> vtxCounts);

  public:
    Wireframe(Shader shader, int xdim, int ydim, int zdim, float xspacing, float yspacing, float zspacing, bool interior = false);
    void setDimension(int xdim, int ydim, int zdim);
    void setSpacing(float xspacing, float yspacing, float zspacing);
    //void setShader(Shader shader);
    std::array<int, 3> getDimension() { return dims; }
    std::array<float, 3> getSpacing() { return spacings; }

    void initWireframeBuffers();
    void drawWireframe();
    void drawOutline();
};

#endif