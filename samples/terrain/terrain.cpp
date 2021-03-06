
#include <gl-application.h>
#include <task.h>
#include <tx-manager.h>
#include <maths.h>
#include <types.h>
#include <gl-renderer.h>
#include <camera.h>

using namespace Dodo;

namespace
{

const char* gVertexShaderSourceDiffuse[] = {
                                            "#version 440 core\n"
                                            "layout (location = 0 ) in vec3 aPosition;\n"
                                            "layout (location = 1 ) in vec2 aTexCoord;\n"
                                            "layout (location = 2 ) in vec3 aNormal;\n"
                                            "uniform sampler2D uTexture;\n"
                                            "out vec2 uv;\n"
                                            "out vec3 normal;\n"
                                            "uniform mat4 uModelViewProjection;\n"
                                            "uniform mat4 uModel;\n"
                                            "uniform float uElevation;\n"
                                            "uniform uvec2 uHeightMapSize;\n"
                                            "uniform uvec2 uMapSize;\n"

                                            "const ivec3 off = ivec3(-1,0,1);\n"
                                            "void main(void)\n"
                                            "{\n"
                                            "  float elevation = textureOffset(uTexture, aTexCoord, ivec2(0,0)).x * uElevation;\n"
                                            "  vec3 position = aPosition + elevation*aNormal;\n"
                                            "  gl_Position = uModelViewProjection * vec4(position,1.0);\n"
                                            "  float right = textureOffset(uTexture, aTexCoord, off.zy).x * uElevation;\n"
                                            "  float up = textureOffset(uTexture, aTexCoord, off.yz).x * uElevation;\n"
                                            "  float left = textureOffset(uTexture, aTexCoord, off.xy).x * uElevation;\n"
                                            "  float down = textureOffset(uTexture, aTexCoord, off.yx).x * uElevation;\n"
                                            "  vec3 tangent = vec3(float(uMapSize.x)/float(uHeightMapSize.x),0.0,0.0);\n"
                                            "  vec3 binormal = vec3(0.0,float(uMapSize.y)/float(uHeightMapSize.y),0.0);\n"
                                            "  vec3 positionRight = ( aPosition  + tangent) + right*aNormal;\n"
                                            "  vec3 positionUp = ( aPosition + binormal)+ up*aNormal;\n"
                                            "  vec3 dx = normalize( positionRight - position);\n"
                                            "  vec3 dy = normalize( positionUp - position );\n"
                                            "  normal = normalize( (uModel * vec4(cross( dx, dy ),0.0)).xyz);\n"
                                            "  uv = aTexCoord;\n"
                                            "}\n"
};
const char* gFragmentShaderSourceDiffuse[] = {
                                              "#version 440 core\n"
                                              "in vec2 uv;\n"
                                              "out vec3 color;\n"
                                              "in vec3 normal;\n"
                                              "uniform vec3 uLightDirection;\n"
                                              "uniform sampler2D uColorTexture;\n"
                                              "void main(void)\n"
                                              "{\n"
                                              "  vec3 lightDirection = normalize(uLightDirection);\n"
                                              "  float diffuse = max(0.0,dot( normal,lightDirection ) );\n"
                                              "  color = texture(uColorTexture, uv ).xyz * (diffuse+0.1);\n"
                                              "}\n"
};


const char* gVertexShaderSkybox[] = {
                                     "#version 440 core\n"
                                     "layout (location = 0 ) in vec3 aPosition;\n"
                                     "layout (location = 1 ) in vec2 aTexCoord;\n"
                                     "uniform mat4 uViewProjectionInverse;\n"
                                     "out vec3 uv;\n"

                                     "void main(void)\n"
                                     "{\n"
                                     "  gl_Position = vec4(aPosition,1.0);\n"
                                     "  vec4 positionInWorldSpace = uViewProjectionInverse*gl_Position;\n"
                                     "  uv = positionInWorldSpace.xyz / positionInWorldSpace.w;\n"
                                     "}\n"
};
const char* gFragmentShaderSkybox[] = {
                                       "#version 440 core\n"
                                       "uniform samplerCube uTexture0;\n"
                                       "in vec3 uv;\n"
                                       "out vec4 color;\n"
                                       "void main(void)\n"
                                       "{\n"
                                       "  color =texture(uTexture0, uv );\n"
                                       "}\n"
};

struct Vertex
{
  Dodo::vec3 position;
  Dodo::vec2 uv;
  Dodo::vec3 normal;
};

}

struct LightAnimation
{
  void Run( f32 timeDelta )
  {
    f32 a = mCurrentTime + (timeDelta * 0.001f);
    if( a > mDuration )
      mCurrentTime = a - mDuration;
    else
      mCurrentTime = a;

    f32 t = mCurrentTime / mDuration;

    f32 angle = t * 2.0f * M_PI;
    Dodo::quat q = Dodo::QuaternionFromAxisAngle( Dodo::vec3(0.0f,1.0f,0.0f), angle );
    mLightDirection = Dodo::Rotate( Dodo::vec3(0.0f,0.0f,1.0f), q );
  }

  Dodo::vec3 mLightDirection;
  f32 mDuration;
  f32 mCurrentTime;
};

class App : public Dodo::GLApplication
{
public:
  App()
:Dodo::GLApplication("Demo",500,500,4,4),
 mTxManager(1),
 mCamera( vec3(0.0f,10.0f,50.0f), vec2(0.0f,0.2f), 1.0f ),
 mElevation(0.0),
 mShader(),
 mGrid(),
 mMousePosition(0.0f,0.0f),
 mMouseButtonPressed(false)
  {}

  ~App()
  {}

  void Init()
  {
    mTxId0 = mTxManager.CreateTransform(Dodo::vec3(0.0f,0.0f,0.0f), Dodo::vec3(1.0f,1.0f,1.0f), Dodo::QuaternionFromAxisAngle( Dodo::vec3(1.0f,0.0f,0.0f), Dodo::DegreeToRadian(-90.0f)));
    mTxManager.Update();
    mProjection = Dodo::ComputePerspectiveProjectionMatrix( Dodo::DegreeToRadian(75.0f),(f32)mWindowSize.x / (f32)mWindowSize.y,1.0f,500.0f );
    ComputeSkyBoxTransform();

    //Create shaders
    mSkyboxShader = mRenderer.AddProgram((const u8**)gVertexShaderSkybox, (const u8**)gFragmentShaderSkybox);
    mShader = mRenderer.AddProgram((const u8**)gVertexShaderSourceDiffuse, (const u8**)gFragmentShaderSourceDiffuse);


    //Load resources
    Dodo::Image image("../resources/terrain-heightmap.bmp");
    mHeightMap = mRenderer.Add2DTexture( image,false );
    mHeightmapSize = Dodo::uvec2(image.mWidth, image.mHeight);
    Dodo::Image colorImage("../resources/terrain-color.jpg");
    mColorMap = mRenderer.Add2DTexture( colorImage,true );
    mMapSize = Dodo::uvec2(100u,100u);
    mElevation = mMapSize.x / 10.0f;
    mGrid = mRenderer.CreateQuad(mMapSize, true, true, mHeightmapSize );

    Dodo::Image cubemapImages[6];
    cubemapImages[0].LoadFromFile( "../resources/sky-box0/xpos.png", false);
    cubemapImages[1].LoadFromFile( "../resources/sky-box0/xneg.png", false);
    cubemapImages[2].LoadFromFile( "../resources/sky-box0/ypos.png", false);
    cubemapImages[3].LoadFromFile( "../resources/sky-box0/yneg.png", false);
    cubemapImages[4].LoadFromFile( "../resources/sky-box0/zpos.png", false);
    cubemapImages[5].LoadFromFile( "../resources/sky-box0/zneg.png", false);
    mCubeMap = mRenderer.AddCubeTexture(&cubemapImages[0]);
    mQuad = mRenderer.CreateQuad(Dodo::uvec2(2u,2u),true,false );

    //Set GL state
    mRenderer.SetCullFace( Dodo::CULL_NONE );
    mRenderer.SetDepthTest( Dodo::DEPTH_TEST_LESS_EQUAL );
    mRenderer.SetClearColor( Dodo::vec4(0.1f,0.0f,0.65f,1.0f));
    mRenderer.SetClearDepth( 1.0f );


    lightAnimation.mDuration = 10.0f;

  }

  void ComputeSkyBoxTransform()
  {
    Dodo::mat4 mViewNoTranslation( mCamera.txInverse );
    mViewNoTranslation.SetTranslation( Dodo::VEC3_ZERO );
    Dodo::ComputeInverse(mViewNoTranslation * mProjection, mSkyBoxTransform );
  }

  void Render()
  {
    mRenderer.ClearBuffers(Dodo::COLOR_BUFFER | Dodo::DEPTH_BUFFER );

    lightAnimation.Run( GetTimeDelta() );
    //Draw skybox
    mRenderer.UseProgram( mSkyboxShader );
    mRenderer.BindCubeTexture( mCubeMap, 0 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mSkyboxShader,"uTexture0"), 0 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mSkyboxShader,"uViewProjectionInverse"), mSkyBoxTransform );
    mRenderer.DisableDepthWrite();
    mRenderer.SetupMeshVertexFormat( mQuad );
    mRenderer.DrawMesh( mQuad );
    mRenderer.EnableDepthWrite();

    //Draw terrain
    Dodo::mat4 modelMatrix;
    mTxManager.GetWorldTransform( mTxId0, &modelMatrix );
    mRenderer.UseProgram( mShader );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uModelViewProjection"), modelMatrix * mCamera.txInverse * mProjection );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uModel"),  modelMatrix  );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uElevation"), mElevation);
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uLightDirection"), lightAnimation.mLightDirection );
    mRenderer.Bind2DTexture( mHeightMap, 0 );
    mRenderer.Bind2DTexture( mColorMap, 1 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uHeightMapSize"), mHeightmapSize );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uMapSize"), mMapSize );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uTexture"), 0 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uColorTexture"), 1 );
    mRenderer.SetupMeshVertexFormat( mGrid );
    mRenderer.DrawMesh( mGrid );
  }

  void OnResize(size_t width, size_t height )
  {

    mRenderer.SetViewport( 0, 0, width, height );
    mProjection = Dodo::ComputePerspectiveProjectionMatrix( 1.5f,(f32)width / (f32)height,0.01f,500.0f );
  }

  void OnKey( Key key, bool pressed )
  {
    static float factor = 1.0f;
    if( pressed )
    {
      switch( key )
      {
        case KEY_UP:
        case 'w':
        {
          mCamera.Move( 0.0f, -1.0f );
          ComputeSkyBoxTransform();
          break;
        }
        case KEY_DOWN:
        case 's':
        {
          mCamera.Move( 0.0f, 1.0f );
          ComputeSkyBoxTransform();
          break;
        }
        case KEY_LEFT:
        case 'a':
        {
          mCamera.Move( 1.0f, 0.0f );
          ComputeSkyBoxTransform();
          break;
        }
        case KEY_RIGHT:
        case 'd':
        {
          mCamera.Move( -1.0f, 0.0f );
          ComputeSkyBoxTransform();
          break;
        }
        case 'z':
        {
          factor-=0.1f;
          mRenderer.SetViewport( mWindowSize.x * (1.0f - factor)/2.0f, mWindowSize.y * (1.0f - factor)/2.0f, mWindowSize.x*factor, mWindowSize.y*factor );
          break;
        }
        case 'x':
        {
          factor+=0.1f;  
          mRenderer.SetViewport( mWindowSize.x * (1.0f - factor)/2.0f, mWindowSize.y * (1.0f - factor)/2.0f, mWindowSize.x*factor, mWindowSize.y*factor );
          break;
        }
        default:
          break;
      }
    }
  }

  void OnMouseButton( MouseButton button, f32 x, f32 y, bool pressed )
  {
    mMouseButtonPressed = pressed;
    mMousePosition.x = x;
    mMousePosition.y = y;
  }

  void OnMouseMove( f32 x, f32 y )
  {
    if( mMouseButtonPressed )
    {
      f32 angleY = (x - mMousePosition.x) * 0.01f;
      f32 angleX = (y - mMousePosition.y) * 0.01f;

      mCamera.Rotate( angleX,angleY );
      ComputeSkyBoxTransform();

      mMousePosition.x = x;
      mMousePosition.y = y;
    }
  }

private:

  Dodo::TxManager mTxManager;
  FreeCamera mCamera;
  Dodo::Id mTxId0;
  f32 mElevation;

  Dodo::ProgramId   mShader;
  Dodo::ProgramId   mSkyboxShader;
  Dodo::MeshId      mGrid;
  Dodo::uvec2       mMapSize;
  Dodo::TextureId  mHeightMap;
  Dodo::uvec2      mHeightmapSize;
  Dodo::TextureId  mColorMap;

  Dodo::MeshId     mQuad;
  Dodo::TextureId  mCubeMap;

  Dodo::mat4 mSkyBoxTransform;
  Dodo::mat4 mProjection;
  Dodo::vec2 mMousePosition;
  bool mMouseButtonPressed;

  LightAnimation lightAnimation;
};

int main()
{
  App app;
  app.MainLoop();

  return 0;
}

