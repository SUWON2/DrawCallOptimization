# DrawCallOptimization
2D 게임에 적합한 기술로 텍스처 아틀라스를 사용하지 않고 한 번에 패스하는 방법을 소개합니다.  
DirectX11와 OpenGLES 3.1에서 사용할 수 있는 방법으로 공유된 코드는 OpenGLES 3.1 API만 사용합니다.  
간단히 PC버전에서만 구동할 수 있도록 코드를 작성했습니다.  
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

[빌드 방법]
솔루션을 열고 x64로 맞춰져 있다면 x86으로 설정해 주세요
성능 테스트할 때는 Debug모드가 아닌 Release모드로 설정해 주세요
DLL 폴더에 있는 파일들을 실행 프로그램으로 복사해 주세요