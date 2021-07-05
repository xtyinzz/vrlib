#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <vrlib/shader.h>
#include <vrlib/Wireframe.h>
#include <vrlib/Point.h>
#include <vrlib/filesystem.h>
#include <vrlib_vr/render.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <iterator>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <chrono>
#include <thread>
/*
TODO:
  //1. get dimension/spacings from args (like vrlib)
  //2. juxtapose wireframe and vrlib side by side with correct transformation angle
    //2.1 update vrlib transformation to keep consistent with OpenGL transformations
  3. DVR with OpenGL
*/

/*
TODO:
  //6/10: debug vrlib rotation vs glm rotation
    //6/17: rotation might be fine, need to incorporate view and projection matrix in vrlib
      //NEED TO LOOK AT: vrlib and glm might have different matrix setup and multiplication logic
      //vrlib: how to get viewing ray in eye space
*/

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;
GLFWwindow* windowTexVR;

bool leftClicking = false;
float xDeg, yDeg, zDeg;
int xdim, ydim, zdim, udim, vdim;
float *volume;
char *outFP, *cmapFP;
std::vector<short> degrees;
std::vector<char> axes;
glm::mat4 mvp;

unsigned int texture;

// volumeRender vr;

// glm::mat4 model(1.f);
// glm::mat4 camModel(1.f);

glm::mat4 initMVP(glm::vec3 trans, glm::vec3 rot, glm::vec3 eyePos, glm::vec3 coi, glm::vec3 up, float fov, float aspect, float zNear, float zFar) {
  // perform model transformation
  glm::mat4 translate = glm::translate(glm::mat4(1.f), trans); // amount to translate
  glm::mat4 rotX = glm::rotate(glm::mat4(1.f), glm::radians(rot.x), glm::vec3(1.f, 0.f, 0.f));
  glm::mat4 rotY = glm::rotate(glm::mat4(1.f), glm::radians(rot.y), glm::vec3(0.f, 1.f, 0.f));
  glm::mat4 rotZ = glm::rotate(glm::mat4(1.f), glm::radians(rot.z), glm::vec3(0.f, 0.f, 1.f));
  //glm::mat4 modelMatrix = translate * rotX * rotY * rotZ;
  glm::mat4 modelMatrix = translate * rotZ * rotY * rotX;

  // std::cout << glm::to_string(glm::mat3(1,2,3,4,5,6,7,8,9)) << "\n\n";
  // std::cout << glm::to_string(translate[1]) << "\n\n";
  // std::cout << glm::to_string(translate[2]) << "\n\n";
  // std::cout << glm::to_string(translate[3]) << "\n\n";

  // get view and projection matrix
  glm::mat4 viewMatrix = glm::lookAt(eyePos, coi, up);

  //glm::mat4 projectionMatrix = glm::perspective(fov, aspect, zNear, zFar);

  // orthographic projection is used to accommodate legacy vrlib transformation
  // to supppurt juxtaposition of bounding box and vr image,
  glm::mat4 projectionMatrix = glm::ortho(-1.75f, 1.75f, -1.75f, 1.75f, zNear,zFar);

  //viewMatrix = glm::mat4(1.f);
  //projectionMatrix = glm::mat4(1.f);
  return projectionMatrix * viewMatrix * modelMatrix;
}

void arg_usage(char* prgm) {
  printf(" usage: %s udim vdim volume colormap alpha beta gamma out \n", 
	 prgm); 
  exit(0); 
}

double normalizeValue(double min, double max, double x) {
  return (x - min) / (max - min);
}

float normalizeValue(float min, float max, float x) {
  return (x - min) / (max - min);
}

unsigned int initTexVAO() {
  float vertices[] = {
      // positions        // texture coords
      0.5f,  0.5f, 0.0f,    1.0f, 1.0f,   // top right
      0.5f, -0.5f, 0.0f,    1.0f, 0.0f,   // bottom right
      -0.5f, -0.5f, 0.0f,    0.0f, 0.0f,   // bottom left
      -0.5f,  0.5f, 0.0f,    0.0f, 1.0f    // top left 
  };

  unsigned int indices[] = {  
      0, 1, 3, // first triangle
      1, 2, 3  // second triangle
  };
  unsigned int VBO, VAO, EBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  // 2D texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  // disable 3D texcoord attribute, so GLSL will give default value of 0.
  glDisableVertexAttribArray(2);

  return VAO;
}

void readPPM(const char *ppmFP, int &width, int &height, GLubyte **data) {
  // read VR image data
  std::ifstream ppmFile{ ppmFP };
  // If we couldn't open the output file stream for reading
  if (!ppmFile)
  {
    // Print an error and exit
    std::cerr << "Uh oh, ppm file could not be opened for reading!" << std::endl;
  }

  std::string magicNum, widthStr, heightStr, maxValStr;
  ppmFile >> magicNum >> widthStr >> heightStr >> maxValStr;

  std::cout << "\nppm property: W - " << 
    widthStr << ", H - " << heightStr << ", max value - " << maxValStr << '\n';

  width = std::stoi(widthStr);
  height = std::stoi(heightStr);

  size_t pixelSize = width*height;
  *data = new GLubyte[pixelSize*3];

  int ppmNewlineOffset = 3;
  for (int i=0; i < pixelSize; ++i) {
    std::string r,g,b;
    ppmFile >> r >> g >> b;
    (*data)[i*3] = (GLubyte) std::stoi(r);
    (*data)[i*3 + 1] = (GLubyte) std::stoi(g);
    (*data)[i*3 + 2] = (GLubyte) std::stoi(b);
    // print some of the lines to verify file reading
    // if (i >= 38639 - ppmNewlineOffset - 1 && i <= 38647 - ppmNewlineOffset - 1) {
    //   std::cout << i + ppmNewlineOffset + 1 << ": " << r << ", " << std::stoi(g) << ", " << std::stoi(b) << '\n';
    // }
  }
}

unsigned int setTexture(const char *ppmFP) {
  // // read VR image data
  // std::ifstream ppmFile{ ppmFP };
  // // If we couldn't open the output file stream for reading
  // if (!ppmFile)
  // {
  //   // Print an error and exit
  //   std::cerr << "Uh oh, ppm file could not be opened for reading!" << std::endl;
  // }

  // std::string magicNum, widthStr, heightStr, maxValStr;
  // ppmFile >> magicNum >> widthStr >> heightStr >> maxValStr;

  // std::cout << "\nppm property: W - " << 
  //   widthStr << ", H - " << heightStr << ", max value - " << maxValStr << '\n';

  // int width, height, maxVal;
  // width = std::stoi(widthStr);
  // height = std::stoi(heightStr);
  // maxVal = std::stoi(maxValStr);

  // size_t pixelSize = width*height;
  // GLubyte *data = new GLubyte[pixelSize*3];

  // int ppmNewlineOffset = 3;
  // for (int i=0; i < pixelSize; ++i) {
  //   std::string r,g,b;
  //   ppmFile >> r >> g >> b;
  //   data[i*3] = (GLubyte) std::stoi(r);
  //   data[i*3 + 1] = (GLubyte) std::stoi(g);
  //   data[i*3 + 2] = (GLubyte) std::stoi(b);
  //   // print some of the lines to verify file reading
  //   // if (i >= 38639 - ppmNewlineOffset - 1 && i <= 38647 - ppmNewlineOffset - 1) {
  //   //   std::cout << i + ppmNewlineOffset + 1 << ": " << r << ", " << std::stoi(g) << ", " << std::stoi(b) << '\n';
  //   // }
  // }

  int width, height;
  GLubyte *data;
  readPPM(ppmFP, width, height, &data);

  // configure and set textsure
  unsigned int texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
  // set the texture wrapping parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // set texture filtering parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // load image, create texture and generate mipmaps
  if (data)
  {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
      glGenerateMipmap(GL_TEXTURE_2D);
  }
  else
  {
      std::cout << "Failed to load texture" << std::endl;
  }
  return texture;
}

void updateTexVR(unsigned int texture, void *data) {

}

std::vector<GLfloat> normalizeArray(float rangeMin, float rangeMax, std::vector<float> numbers) {
  const float maxNum = *std::max_element(numbers.begin(), numbers.end());
  const float minNum = *std::min_element(numbers.begin(), numbers.end());

  size_t length = numbers.size();
  std::vector<GLfloat> result;
  result.reserve(length);

  for(int i = 0; i < length; ++i) {
    result[i] = rangeMin + (rangeMax - rangeMin) * normalizeValue(minNum, maxNum, numbers[i]);
  }

  // std::cout << numbers[length-1] << " VS " << result[length-1] << "\n";
  // std::cout << numbers[length-5002] << " VS " << result[length-5002] << "\n";
  // std::cout << numbers[length-435121] << " VS " << result[length-435121] << "\n";
  // std::cout << numbers[length-2021122] << " VS " << result[length-2021122] << "\n";

  const GLfloat maxNorm = *std::max_element(result.begin(), result.end());
  const GLfloat minNorm = *std::min_element(result.begin(), result.end());

  std::cout << "Max element: " << maxNum << ", Min element: " << minNum << "\n" << length << "\n";
  std::cout << "Max normed: " << maxNorm << ", Min normed: " << minNorm << " " << (maxNorm == minNorm) << "\n";

  return result;
}

unsigned int setVolTexture(float volume[], int width, int height, int depth) {
  // configure and set texture
  unsigned int texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_3D, texture); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
  // set the texture wrapping parameters
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // set texture filtering parameters
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  size_t volLength = width * height * depth;
  std::vector <GLfloat> volNormed = normalizeArray(0, 1, std::vector<float>(volume, volume + volLength));

  // load image, create texture and generate mipmaps
  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0, GL_RED, GL_FLOAT, volNormed.data());
  glGenerateMipmap(GL_TEXTURE_3D);

  return texture;
}

// glm::vec3 getBallVec(double mouseX, double mouseY, double ballRadius, glm::vec3 center) {
//   double ballX, ballY, ballZ;
//   double center = 0.5;
//   ballX = normalizeValue(0, 1, mouseX);
//   ballY = normalizeValue(0, 1, mouseY);
//   ballZ = sqrt((ballRadius*ballRadius - ballY*ballY) - ballX*ballX); // pythagorean theorem twice to get Z coord (on the ball)

//   return glm::vec3(ballX, ballY, ballZ) - center;
// }

// // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Arcball
// void getArcballRotation(GLFWwindow* window) {
//   double mouseX, mouseY;
//   glfwGetCursorPos(window, &mouseX, &mouseY);
//   std::cout << "X: " << mouseX << "Y: " << mouseY << "\n";

//   glm::vec3 center(0.5f, 0.5f, 0.f);
  
//   glm::vec3 start, end;
// }

// void mouseCallback(GLFWwindow* window, int button, int action, int mods)
// {
//     if (button == GLFW_MOUSE_BUTTON_LEFT) {
//         if(GLFW_PRESS == action)
//             leftClicking = true;
//         else if(GLFW_RELEASE == action)
//             leftClicking = false;
//     }

//     if(leftClicking) {
//          // do your drag here
//     }
// }

// void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
//   std::cout << "I'm called\n";
// }


// ***********************************************************************************************************************8
void keyCallbackWF(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  float deg = 4.f;

  if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
    std::cout << "ENTER pressed! equal size deg vs axis: " << (degrees.size() == axes.size()) <<
    " # of rotations: " << degrees.size()  << "\n";

    // generate new vr image from user operation
    volumeRender vr(xdim,ydim,zdim,udim,vdim,volume); 
    vr.readCmapFile(cmapFP); 
    vr.set_view(xDeg, yDeg, zDeg); 
    vr.update_rotation(degrees, axes);
    vr.execute(); 
    vr.out_to_image(outFP);

    // update the binded VR texture
    // int width, height;
    // GLubyte *data;
    // readPPM(outFP, width, height, &data);

    texture = setTexture(outFP);
    // glClearColor(0.f, 1.f, 0.f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // std::chrono::seconds dura(3);
    // std::this_thread::sleep_for( dura );
    // glfwMakeContextCurrent(windowTexVR);
    // glBindTexture(GL_TEXTURE_2D, texture);
    // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
    // glGenerateMipmap(GL_TEXTURE_2D);

  } else if (key == GLFW_KEY_Q && action != GLFW_RELEASE) {
    mvp *= glm::rotate(glm::mat4(1.f), glm::radians(deg), glm::vec3(1.f, 0.f, 0.f));
    degrees.push_back((short) deg);
    axes.push_back('x');
  } else if (key == GLFW_KEY_W && action != GLFW_RELEASE) {
    mvp *= glm::rotate(glm::mat4(1.f), glm::radians(deg), glm::vec3(0.f, 1.f, 0.f));
    degrees.push_back((short) deg);
    axes.push_back('y');
  } else if (key == GLFW_KEY_E && action != GLFW_RELEASE) {
    mvp *= glm::rotate(glm::mat4(1.f), glm::radians(deg), glm::vec3(0.f, 0.f, 1.f));
    degrees.push_back((short) deg);
    axes.push_back('z');
  } else if (key == GLFW_KEY_A && action != GLFW_RELEASE) {
    mvp *= glm::rotate(glm::mat4(1.f), glm::radians(-deg), glm::vec3(1.f, 0.f, 0.f));
    degrees.push_back((short) -deg);
    axes.push_back('x');
  } else if (key == GLFW_KEY_S && action != GLFW_RELEASE) {
    mvp *= glm::rotate(glm::mat4(1.f), glm::radians(-deg), glm::vec3(0.f, 1.f, 0.f));
    degrees.push_back((short) -deg);
    axes.push_back('y');
  } else if (key == GLFW_KEY_D && action != GLFW_RELEASE) {
    mvp *= glm::rotate(glm::mat4(1.f), glm::radians(-deg), glm::vec3(0.f, 0.f, 1.f));
    degrees.push_back((short) -deg);
    axes.push_back('z');
  }

// ***********************************************************************************************************************8
}

void keyCallbackVR(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
    std::cout << "ENTER pressed! equal size deg vs axis: " << (degrees.size() == axes.size()) <<
    " # of rotations: " << degrees.size()  << "\n";

    // generate new vr image from user operation
    volumeRender vr(xdim,ydim,zdim,udim,vdim,volume); 
    vr.readCmapFile(cmapFP); 
    vr.set_view(xDeg, yDeg, zDeg);
    vr.update_rotation(degrees, axes);
    vr.execute();
    vr.out_to_image(outFP);

    // update the binded VR texture
    // int width, height;
    // GLubyte *data;
    // readPPM(outFP, width, height, &data);

    texture = setTexture(outFP);
    // glClearColor(0.f, 1.f, 0.f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // std::chrono::seconds dura(3);
    // std::this_thread::sleep_for( dura );
    // glfwMakeContextCurrent(windowTexVR);
    // glBindTexture(GL_TEXTURE_2D, texture);
    // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
    // glGenerateMipmap(GL_TEXTURE_2D);

  }

// ***********************************************************************************************************************8
}

float normalizeAngle(float deg) {
  while(deg < 0)   deg += 360.0f;
  while(deg > 360) deg -= 360.0f;
  return deg;
}
//
int main(int argc, char *argv[])
{
  // print cwd
  char tmp[256];
  getcwd(tmp, 256);
  std::cout << "Current working directory (make sure it's the location of the executable): " << tmp << '\n';  
  std::cout << argc << " arguments given\n";

  // process args
  if (argc!= 9) arg_usage(argv[0]); 

  udim = atoi(argv[1]); 
  vdim = atoi(argv[2]); 

  xDeg = (float) std::stof(argv[5]); 
  yDeg = (float) std::stof(argv[6]); 
  zDeg = (float) std::stof(argv[7]);
  std::cout << "Rotation (before): " << xDeg << " " << yDeg << " " << zDeg << "\n";

  xDeg = normalizeAngle(xDeg);
  yDeg = normalizeAngle(yDeg);
  zDeg = normalizeAngle(zDeg);

  std::cout << "Rotation (after): " << xDeg << " " << yDeg << " " << zDeg << "\n";

  char *volFP = argv[3];
  cmapFP = argv[4];

  outFP = argv[8];

  FILE* volF;
  volF = fopen(volFP,"r"); 
  if (volF == NULL) {
    printf(" can't open volume file %s\n", volFP); 
    exit(0);
  }

  printf(" read volume file %s ....\n", volFP); 

  fread(&xdim, sizeof(int), 1, volF);
  fread(&ydim, sizeof(int), 1, volF);
  fread(&zdim, sizeof(int), 1, volF);

  printf(" %d %d %d\n", xdim, ydim, zdim); 

  int sizeVol = xdim*ydim*zdim;
  volume = new float[sizeVol];
  fread(volume, sizeof(float), sizeVol, volF);

  volumeRender vr(xdim,ydim,zdim,udim,vdim,volume); 
  vr.readCmapFile(cmapFP); 
  vr.set_view(xDeg, yDeg, zDeg); 
  vr.execute(); 
  vr.out_to_image(outFP);

  /////////////////////////////////////////////////////////////////////////////////////////
  // glfw: initialize and configure
  // ------------------------------
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  int xpos, ypos, height;
  glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &xpos, &ypos, NULL, &height);
// #ifdef __APPLE__
//   glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
// #endif

  ////////////////////////////////////////////////////////////////////////////////////////
  // glfw window creation
  // --------------------
  // create Wireframe window and GL context and create associated GL objects
  GLFWwindow* windowWF = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "vrlib - Bounding Box View", NULL, NULL);
  if (windowWF == NULL)
  {
    std::cout << "Failed to create GLFW window for Wireframe" << std::endl;
    glfwTerminate();
    return -1;
  }

  glfwSetWindowPos(windowWF,
                    xpos,
                    ypos);
  glfwSetInputMode(windowWF, GLFW_STICKY_KEYS, GLFW_TRUE);
  glfwMakeContextCurrent(windowWF);
  // glad: load all OpenGL function pointers (for VR window)
  // ---------------------------------------
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD for Wireframe Window" << std::endl;
    return -1;
  }
  glfwSetFramebufferSizeCallback(windowWF, framebuffer_size_callback);
  glLineWidth(1.0f);
  // configure global depth test
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);
  // glfwSetMouseButtonCallback(windowWF, mouseCallback);
  // glfwSetScrollCallback(windowWF, scrollCallback);
  glfwSetKeyCallback(windowWF, keyCallbackWF);


  // setup vbo, ebo, vao with Wireframe gl contexts
  // wireframe view
  Shader shaderWireframe("shader/shader.vert", "shader/shader.frag");
  float xspacing = 1.f;
  float yspacing = 1.f;
  float zspacing = 1.f;
  Wireframe wireframe(shaderWireframe, xdim, ydim, zdim, xspacing, yspacing, zspacing);
  // point volume view
  Shader shaderPointVol("shader/shader_tex_point.vert", "shader/shader_tex_point.frag");
  unsigned int volumeTex = setVolTexture(volume, xdim, ydim, zdim);
  Point pointVolume(shaderPointVol, xdim, ydim, zdim, xspacing, yspacing, zspacing);
  
  glfwMakeContextCurrent(NULL);


  // create Wireframe window and GL context and create associated GL objects
  //glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
  windowTexVR = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "vrlib - VR View", NULL, NULL);
  if (windowTexVR == NULL)
  {
    std::cout << "Failed to create GLFW window for VR image" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwSetWindowPos(windowTexVR,
                    xpos + SCR_WIDTH,
                    ypos);
  glfwSetInputMode(windowTexVR, GLFW_STICKY_KEYS, GLFW_TRUE);

  glfwMakeContextCurrent(windowTexVR);
  // glad: load all OpenGL function pointers (for VR window)
  // ---------------------------------------
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD for VR image Window" << std::endl;
    return -1;
  }
  glfwSetFramebufferSizeCallback(windowTexVR, framebuffer_size_callback);
  glfwSetKeyCallback(windowTexVR, keyCallbackVR);
  // configure global depth test
  glEnable(GL_DEPTH_TEST);

  Shader shaderTextureVR("shader/shader_tex.vert", "shader/shader_tex_vr.frag");
  // setup vbo, ebo, vao with VR image gl contexts

  texture = setTexture(outFP);
  unsigned int texVAO = initTexVAO();
  shaderTextureVR.use();
  shaderTextureVR.setInt("vrTexture", 0); // set texture unit manually
  
  glfwShowWindow(windowWF);
  // glfwShowWindow(windowTexVR);

  /////////////////////////////////////////////////////////////////////////////////////////
  // setup MVP matrices
  // xDeg = -45.f + 360;
  // yDeg = 45.f;
  // zDeg = 0.f;
  const glm::vec3 initTrans(0.f, 0.f, -4.f);
  const glm::vec3 initRot(xDeg, yDeg, zDeg);

  const glm::vec3 eyePos(0.f, 0.0f, 0.f);
  const glm::vec3 coi(0.f, 0.f, -5.f);
  const glm::vec3 up(0.f, 5.f, 0.f);

  const float fieldOfView = glm::radians(45.f);
  const float aspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
  const float zNear = 0.1;
  const float zFar = 100.0;

  mvp = initMVP(initTrans, initRot, eyePos, coi, up, fieldOfView, aspect, zNear, zFar);
  //std::cout << glm::to_string(mvp) << "\n\n";

  double mouseX, mouseY;

  // render loop
  // -----------
  while (!glfwWindowShouldClose(windowWF) && !glfwWindowShouldClose(windowTexVR))
  {

    //getArcballRotation(windowWF);

    // render at Wireframe Window
    glfwMakeContextCurrent(windowWF);
    processInput(windowWF);
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderWireframe.use();
    shaderWireframe.setMat4("mvp", mvp);
    wireframe.drawOutline();
    //wireframe.drawWireframe();

    // shaderPointVol.use();
    // shaderPointVol.setMat4("mvp", mvp);
    // pointVolume.drawPoint();

    glfwSwapBuffers(windowWF);

    // render at VR window
    glfwMakeContextCurrent(windowTexVR);
    processInput(windowTexVR);
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    shaderTextureVR.use();
    glBindVertexArray(texVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
    // -------------------------------------------------------------------------------
    glfwSwapBuffers(windowTexVR);
    glfwWaitEvents();
  }

  // glfw: terminate, clearing all previously allocated GLFW resources.
  // ------------------------------------------------------------------
  glfwTerminate();
  return 0;
}


// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
  if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
  // make sure the viewport matches the new window dimensions; note that width and 
  // height will be significantly larger than specified on retina displays.
  glViewport(0, 0, width, height);
}