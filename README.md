# DrawCallOptimization
2D 게임에 적합한 기술로 텍스처 아틀라스를 사용하지 않고 한 번에 패스하는 방법을 소개합니다.  
DirectX11와 OpenGLES 3.1에서 사용할 수 있는 방법으로 공유된 코드는 OpenGLES 3.1 API만 사용합니다.  
알고리즘만 소개해 드리기 위해 간단히 PC버전에서만 구동할 수 있도록 코드를 작성했습니다.  
따라서 모바일로 포팅을 원하신 경우 코드를 적절하게 수정하셔야 됩니다!  

[사용한 라이브러리]  
OpenGLES 3.1  
GLFW  
glm  
Mail OpenGL ES Emulator  
  
[주요 기술]  
Instancing Rendering  
Texture Array  
Astc Compression  
  
[빌드할 수 있는 환경]  
Visual Studio 2017  
Visual Studio 2019  
