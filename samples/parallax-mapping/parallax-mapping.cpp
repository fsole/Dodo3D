
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
                                            "out vec3 lightDirection_tangentspace;\n"
                                            "out vec2 uv;\n"
                                            "out vec3 viewVector_tangentspace;\n"
                                            "uniform mat4 uModelViewProjection;\n"
                                            "uniform mat4 uModelView;\n"
                                            "uniform mat4 uView;\n"
                                            "void main(void)\n"
                                            "{\n"
                                            "  gl_Position = uModelViewProjection * vec4(aPosition,1.0);\n"
                                            "  uv = aTexCoord;\n"
                                            "  vec3 lightDirection_cameraspace = (uView * vec4(normalize(vec3(0.0,1.0,0.0)),0.0)).xyz;\n"
                                            "  vec3 vertexNormal_cameraspace = (uModelView * vec4(0.0,0.0,1.0,0.0)).xyz;\n"
                                            "  vec3 vertexTangent_cameraspace = (uModelView * vec4(1.0,0.0,0.0,0.0)).xyz;\n"
                                            "  vec3 vertexBitangent_cameraspace = (uModelView * vec4(0.0,1.0,0.0,0.0)).xyz;\n"
                                            "  mat3 TBN = transpose(mat3(vertexTangent_cameraspace,vertexBitangent_cameraspace, vertexNormal_cameraspace));\n"
                                            "  lightDirection_tangentspace = TBN * lightDirection_cameraspace;\n"
                                            "  vec3 viewVector = normalize( uModelView * -vec4(aPosition,1.0)).xyz;\n"
                                            "  viewVector_tangentspace = TBN * viewVector;\n"
                                            "}\n"
};
const char* gFragmentShaderSourceDiffuse[] = {
                                              "#version 440 core\n"
                                              "in vec2 uv;\n"
                                              "out vec3 color;\n"
                                              "uniform mat4 uModelView;\n"
                                              "in vec3 lightDirection_tangentspace;\n"
                                              "in vec3 viewVector_tangentspace;\n"
                                              "uniform vec2 uScaleBias;\n"
                                              "uniform sampler2D uNormalMap;\n"
                                              "uniform sampler2D uHeightMap;\n"
                                              "uniform sampler2D uColorMap;\n"
                                              "uniform sampler2D uSpecularMap;\n"
                                              "void main(void)\n"
                                              "{\n"
                                              "  float height = texture( uHeightMap, uv ).r;\n"
                                              "  float v = height * uScaleBias.x - uScaleBias.y;\n"
                                              "  vec2 newUv = uv + (viewVector_tangentspace.xy*v);\n"
                                              "  vec3 normal_tagentspace = normalize(texture( uNormalMap, newUv ).rgb*2.0 - 1.0);\n"
                                              "  vec3 reflection = normalize( reflect( viewVector_tangentspace, normal_tagentspace));\n"
                                              " float specular = pow(max(dot(reflection,lightDirection_tangentspace),0.0),1.0) * texture(uSpecularMap,newUv).r;\n"
                                              "  float diffuse = max(0.0,dot( normal_tagentspace,normalize(lightDirection_tangentspace) ) );\n"
                                              "  color = texture(uColorMap,newUv).rgb * vec3(diffuse,diffuse,diffuse) + vec3(specular,specular,specular);\n"
                                              "}\n"
};
}

class App : public Dodo::GLApplication
{
public:
  App()
  :Dodo::GLApplication("Demo",500,500,4,4),
   mTxManager(1),
   mScale(0.04f),
   mBias(0.02f),
   mShader(),
   mQuad(),
   mCamera(Dodo::vec3(0.0f,6.0f,8.0f), Dodo::vec2(0.0f,0.0f), 1.0f ),
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

    //Load resources
    mShader = mRenderer.AddProgram((const u8**)gVertexShaderSourceDiffuse, (const u8**)gFragmentShaderSourceDiffuse);
    Dodo::Image normalImage("../resources/bric_normal.png");
    mNormalMap = mRenderer.Add2DTexture( normalImage,true );
    Dodo::Image colorImage("../resources/bric_diffuse.png");
    mColorMap = mRenderer.Add2DTexture( colorImage,true );
    Dodo::Image heightImage("../resources/bric_height.png");
    mHeightMap = mRenderer.Add2DTexture( heightImage,true );
    Dodo::Image specularImage("../resources/bric_specular.png");
    mSpecularMap = mRenderer.Add2DTexture( specularImage,true );
    mQuad = mRenderer.CreateQuad(Dodo::uvec2(10u,10u), true, true );

    //Set GL state
    mRenderer.SetCullFace( Dodo::CULL_NONE );
    mRenderer.SetDepthTest( Dodo::DEPTH_TEST_LESS_EQUAL );
    mRenderer.SetClearColor( Dodo::vec4(0.1f,0.0f,0.65f,1.0f));
    mRenderer.SetClearDepth( 1.0f );
  }

  void Render()
  {
    mRenderer.ClearBuffers(Dodo::COLOR_BUFFER | Dodo::DEPTH_BUFFER );

    //Draw mesh
    Dodo::mat4 modelMatrix;
    mTxManager.GetWorldTransform( mTxId0, &modelMatrix );
    mRenderer.UseProgram( mShader );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uModelViewProjection"), modelMatrix * mCamera.txInverse * mProjection );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uView"), mCamera.txInverse );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uModelView"),  modelMatrix * mCamera.txInverse );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uScaleBias"),  Dodo::vec2(mScale,mBias) );
    mRenderer.Bind2DTexture( mColorMap, 0 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uColorMap"), 0 );
    mRenderer.Bind2DTexture( mNormalMap, 1 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uNormalMap"), 1 );
    mRenderer.Bind2DTexture( mHeightMap, 2 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uHeightMap"), 2 );
    mRenderer.Bind2DTexture( mSpecularMap, 3 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShader,"uSpecularMap"), 3 );
    mRenderer.SetupMeshVertexFormat( mQuad );
    mRenderer.DrawMesh( mQuad );
  }

  void OnResize(size_t width, size_t height )
  {
    mRenderer.SetViewport( 0, 0, width, height );
    mProjection = Dodo::ComputePerspectiveProjectionMatrix( 1.5f,(f32)width / (f32)height,0.01f,500.0f );
  }

  void OnKey( Dodo::Key key, bool pressed )
  {
    if( pressed )
    {
      switch( key )
      {
        case Dodo::KEY_UP:
        case 'w':
        {
          mCamera.Move( 0.0f, -1.0f );
          break;
        }
        case Dodo::KEY_DOWN:
        case 's':
        {
          mCamera.Move( 0.0f, 1.0f );
          break;
        }
        case Dodo::KEY_LEFT:
        case 'a':
        {
          mCamera.Move( 1.0f, 0.0f );
          break;
        }
        case Dodo::KEY_RIGHT:
        case 'd':
        {
          mCamera.Move( -1.0f, 0.0f );
          break;
        }
        case 'z':
        {
          mScale += 0.005f;
          break;
        }
        default:
          break;
      }
    }
  }

  void OnMouseButton( Dodo::MouseButton button, f32 x, f32 y, bool pressed )
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
      mMousePosition.x = x;
      mMousePosition.y = y;
    }
  }

private:

  Dodo::TxManager mTxManager;
  Dodo::Id mTxId0;
  f32 mScale;
  f32 mBias;
  Dodo::ProgramId   mShader;
  Dodo::MeshId      mQuad;
  Dodo::TextureId  mColorMap;
  Dodo::TextureId  mNormalMap;
  Dodo::TextureId  mHeightMap;
  Dodo::TextureId  mSpecularMap;

  FreeCamera mCamera;
  Dodo::mat4 mProjection;
  Dodo::vec2 mMousePosition;
  bool mMouseButtonPressed;
};

int main()
{
  App app;
  app.MainLoop();

  return 0;
}
