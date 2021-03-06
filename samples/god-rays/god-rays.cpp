
#include <gl-application.h>
#include <maths.h>
#include <types.h>
#include <camera.h>

using namespace Dodo;

namespace
{

const char* gVSFirstPass[] = {
                              "#version 440 core\n"
                              "layout (location = 0 ) in vec3 aPosition;\n"
                              "layout (location = 1 ) in vec2 aTexCoord;\n"
                              "out vec2 uv;\n"
                              "uniform mat4 uModelViewProjection;\n"
                              "void main(void)\n"
                              "{\n"
                              "  gl_Position = uModelViewProjection * vec4(aPosition,1.0);\n"
                              "  uv = aTexCoord;\n"
                              "}\n"
};
const char* gFSFirstPass[] = {
                              "#version 440 core\n"
                              "in vec2 uv;\n"
                              "out vec3 color;\n"
                              "uniform vec3 uEmissiveColor;\n"
                              "void main(void)\n"
                              "{\n"
                              "  color = uEmissiveColor;\n"
                              "}\n"
};

const char* gVSDiffuse[] = {
                            "#version 440 core\n"
                            "layout (location = 0 ) in vec3 aPosition;\n"
                            "layout (location = 1 ) in vec2 aTexCoord;\n"
                            "out vec2 uv;\n"
                            "uniform mat4 uModelViewProjection;\n"
                            "void main(void)\n"
                            "{\n"
                            "  gl_Position = uModelViewProjection * vec4(aPosition,1.0);\n"
                            "  uv = aTexCoord;\n"
                            "}\n"
};
const char* gFSDiffuse[] = {
                            "#version 440 core\n"
                            "in vec2 uv;\n"
                            "out vec3 color;\n"
                            "uniform sampler2D uColorMap;\n"
                            "uniform vec3 uEmissiveColor;\n"
                            "void main(void)\n"
                            "{\n"
                            "  color = texture(uColorMap,uv).rgb + uEmissiveColor;\n"
                            "}\n"
};

const char* gVSGodRays[] = {
                            "#version 440 core\n"
                            "out vec2 uv;\n"
                            "void main(void)\n"
                            "{\n"
                            "  uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);"
                            "  gl_Position = vec4(uv * vec2(2.0f, 2.0f) + vec2(-1.0f, -1.0f), 0.0f, 1.0f);\n"
                            "}\n"
};

const char* gFSGodRays[] = {
                            "#version 440 core\n"
                            "uniform sampler2D uTexture0;\n"
                            "uniform vec3 uLightScreenSpace;\n"
                            "in vec2 uv;\n"
                            "out vec4 color;\n"
                            "int NUM_SAMPLES = 100;\n"
                            "void main(void)\n"
                            "{\n"
                            "float uDensity = 1.0;\n"
                            "float uWeight = 0.6;\n"
                            "float uDecay = 0.99f;\n"
                            "float uExposure = 0.034;\n"
                            "vec2 lightPosition = uLightScreenSpace.xy;\n"
                            "vec2 deltaTexCoord = ( (2.0 * uv - 1.0) - lightPosition.xy) / float(NUM_SAMPLES) * uDensity;\n"
                            "vec2 texCoord = uv;\n"
                            "color = texture(uTexture0, uv );\n"
                            "float illuminationDecay = 1.0f;\n"
                            "for (int i = 0; i < NUM_SAMPLES; i++)  \n"
                            "{  \n"
                            "  texCoord -= deltaTexCoord; \n"
                            "  vec4 textureColor = texture(uTexture0, texCoord ) * illuminationDecay * uWeight;\n"
                            "  color += textureColor;\n"
                            "  illuminationDecay *= uDecay;  \n"
                            "}  \n"
                            "color *= uExposure;\n"
                            "}\n"
};

}


class App : public Dodo::GLApplication
{
public:

  App()
  :Dodo::GLApplication("Demo",800,800,4,4),
   mCamera(Dodo::vec3(0.0f,10.0f,36.0f), Dodo::vec2(0.0f,0.0f), 1.0f ),
   mMousePosition(0.0f,0.0f),
   mMouseButtonPressed(false)
   {}

  ~App()
  {}

  void Init()
  {
    //Create transformations for the object and the sun
    mObjectTx = Dodo::ComputeTransform(Dodo::vec3(0.0f,0.0f,0.0f), Dodo::vec3(1.0f,1.0f,1.0f), Dodo::QuaternionFromAxisAngle( vec3(0.0f,1.0f,0.0f), Dodo::DegreeToRadian(140.0f)) );
    mLightTx = Dodo::ComputeTransform(Dodo::vec3(0.0f,60.0f,-120.0f), Dodo::vec3(13.0f,13.0f,13.0f), Dodo::QUAT_UNIT );

    //Create shaders
    mShaderFirstPass = mRenderer.AddProgram((const u8**)gVSFirstPass, (const u8**)gFSFirstPass);
    mShaderDiffuse = mRenderer.AddProgram((const u8**)gVSDiffuse, (const u8**)gFSDiffuse);
    mShaderGodRays = mRenderer.AddProgram((const u8**)gVSGodRays, (const u8**)gFSGodRays);

    //Load assets
    Dodo::Image colorImage("../resources/diffuse.jpg");
    mColorMap = mRenderer.Add2DTexture( colorImage,true );
    mMesh = mRenderer.AddMeshFromFile( "../resources/farmhouse.obj" );
    mSun = mRenderer.AddMeshFromFile( "../resources/sphere.obj" );

    //Create fbo for off screen rendering
    mFbo =  mRenderer.AddFrameBuffer();
    mColorAttachment = mRenderer.Add2DTexture( Image( mWindowSize.x, mWindowSize.y, 0, FORMAT_RGBA8 ));
    mDepthStencilAttachment = mRenderer.Add2DTexture( Image( mWindowSize.x, mWindowSize.y, 0, FORMAT_GL_DEPTH_STENCIL ));
    mRenderer.Attach2DColorTextureToFrameBuffer( mFbo, 0, mColorAttachment );
    mRenderer.AttachDepthStencilTextureToFrameBuffer( mFbo, mDepthStencilAttachment );

    //Set GL state
    mRenderer.SetCullFace( Dodo::CULL_BACK );
    mRenderer.SetDepthTest( Dodo::DEPTH_TEST_LESS_EQUAL );
    mRenderer.SetClearDepth( 1.0f );
  }

  void Render()
  {
    /*
     * Render to an off-screen buffer the occluders and the light
     */
    mRenderer.BindFrameBuffer( mFbo );
    mRenderer.SetClearColor( Dodo::vec4(0.0f,0.0f,0.0f,1.0f));
    mRenderer.ClearBuffers(Dodo::COLOR_BUFFER | Dodo::DEPTH_BUFFER );

    //Occluders
    Dodo::mat4 objectMVP = mObjectTx * mCamera.txInverse * mProjection;
    mRenderer.UseProgram( mShaderFirstPass );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderFirstPass,"uModelViewProjection"), objectMVP  );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderFirstPass,"uEmissiveColor"), vec3(0.0f,0.0f,0.0f) );
    mRenderer.SetupMeshVertexFormat( mMesh );
    mRenderer.DrawMesh( mMesh );

    //Light
    Dodo::mat4 lightMVP = mLightTx * mCamera.txInverse * mProjection;
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderFirstPass,"uModelViewProjection"), lightMVP );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderFirstPass,"uEmissiveColor"), vec3(1.0f,1.0f,1.0f) );
    mRenderer.SetupMeshVertexFormat( mSun );
    mRenderer.DrawMesh( mSun );


    /*
     * Render the scene to the on-screen frame buffer
     */
    mRenderer.SetClearColor( Dodo::vec4(0.1f,0.1f,0.7f,1.0f));
    mRenderer.BindFrameBuffer( 0 );
    mRenderer.ClearBuffers(Dodo::COLOR_BUFFER | Dodo::DEPTH_BUFFER );
    mRenderer.UseProgram( mShaderDiffuse );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderDiffuse,"uModelViewProjection"), objectMVP );
    mRenderer.Bind2DTexture( mColorMap, 0 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderDiffuse,"uTexture"), 0 );
    mRenderer.SetupMeshVertexFormat( mMesh );
    mRenderer.DrawMesh( mMesh );

    /**
     * Render crepuscular rays on top with additive blending
     */
    mRenderer.SetBlendingMode( Dodo::BLEND_ADD );
    mRenderer.SetBlendingFunction( BLENDING_FUNCTION_ONE, BLENDING_FUNCTION_ONE, BLENDING_FUNCTION_ONE, BLENDING_FUNCTION_ONE);
    mRenderer.UseProgram( mShaderGodRays );
    mRenderer.Bind2DTexture( mColorAttachment, 0 );
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderGodRays,"uTexture"), 0 );
    vec3 lightPositionScreenSpace = vec3(lightMVP[12]/lightMVP[15],lightMVP[13]/lightMVP[15],lightMVP[14]/lightMVP[15]);
    mRenderer.SetUniform( mRenderer.GetUniformLocation(mShaderGodRays,"uLightScreenSpace"), lightPositionScreenSpace );
    mRenderer.DrawCall( 3 );
    mRenderer.SetBlendingMode( Dodo::BLEND_DISABLED );
  }

  void OnResize(size_t width, size_t height )
  {
    mRenderer.SetViewport( 0, 0, width, height );
    mProjection = Dodo::ComputePerspectiveProjectionMatrix( 1.5f,(f32)width / (f32)height,0.01f,500.0f );

    //Recreate frame buffer images to match new window size
    mRenderer.RemoveTexture(mColorAttachment);
    mRenderer.RemoveTexture(mDepthStencilAttachment);
    mColorAttachment = mRenderer.Add2DTexture( Image( width, height, 0, FORMAT_RGBA8 ));
    mDepthStencilAttachment = mRenderer.Add2DTexture( Image( width, height, 0, FORMAT_GL_DEPTH_STENCIL ));
    mRenderer.Attach2DColorTextureToFrameBuffer( mFbo, 0, mColorAttachment );
    mRenderer.AttachDepthStencilTextureToFrameBuffer( mFbo, mDepthStencilAttachment );
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

      mCamera.Rotate( angleX, angleY );
      mMousePosition.x = x;
      mMousePosition.y = y;
    }
  }

private:

  FreeCamera mCamera;

  Dodo::mat4 mObjectTx;
  Dodo::mat4 mLightTx;

  Dodo::MeshId  mMesh;
  Dodo::MeshId mSun;
  Dodo::TextureId  mColorMap;

  Dodo::ProgramId mShaderFirstPass;
  Dodo::ProgramId mShaderGodRays;
  Dodo::ProgramId mShaderDiffuse;

  FBOId mFbo;
  TextureId mColorAttachment;
  TextureId mDepthStencilAttachment;

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
