/*****************************************************************************/
/**
 *  @file   StylizedLineRenderer.cpp
 *  @author Naohisa Sakamoto
 */
/*----------------------------------------------------------------------------
 *
 *  Copyright (c) Visualization Laboratory, Kyoto University.
 *  All rights reserved.
 *  See http://www.viz.media.kyoto-u.ac.jp/kvs/copyright/ for details.
 *
 *  $Id$
 */
/*****************************************************************************/
#include "StylizedLineRenderer.h"
#include <kvs/OpenGL>
#include <kvs/ProgramObject>
#include <kvs/ShaderSource>
#include <kvs/VertexShader>
#include <kvs/FragmentShader>


namespace
{

/*===========================================================================*/
/**
 *  @brief  Returns vertex-color array.
 *  @param  line [in] pointer to the line object
 */
/*===========================================================================*/
kvs::ValueArray<kvs::UInt8> VertexColors( const kvs::LineObject* line )
{
    if ( line->colorType() == kvs::LineObject::VertexColor ) { return line->colors(); }

    const size_t nvertices = line->numberOfVertices();
    const kvs::RGBColor color = line->color();

    kvs::ValueArray<kvs::UInt8> colors( nvertices * 3 );
    for ( size_t i = 0; i < nvertices; i++ )
    {
        colors[ 3 * i + 0 ] = color.r();
        colors[ 3 * i + 1 ] = color.g();
        colors[ 3 * i + 2 ] = color.b();
    }

    return colors;
}

/*===========================================================================*/
/**
 *  @brief  Returns vertex normal array.
 *  @param  line [in] pointer to the line object
 */
/*===========================================================================*/
kvs::ValueArray<kvs::Real32> VertexNormals( const kvs::LineObject* line )
{
    if ( line->coords().size() == line->normals().size() ) { return line->normals(); }

    kvs::ValueArray<kvs::Real32> normals( line->coords().size() );
    switch ( line->lineType() )
    {
    case kvs::LineObject::Uniline:
    {
        const size_t nconnections = line->numberOfConnections();
        for ( size_t i = 0; i < nconnections - 1; i++ )
        {
            const size_t id0 = line->connections().at( i );
            const size_t id1 = line->connections().at( i + 1 );
            const kvs::Vec3 p0 = line->coord( id0 );
            const kvs::Vec3 p1 = line->coord( id1 );
            const kvs::Vec3 n = ( p1 - p0 ).normalized();
            normals[ 3 * id0 + 0 ] = n.x();
            normals[ 3 * id0 + 1 ] = n.y();
            normals[ 3 * id0 + 2 ] = n.z();
            if ( i == nconnections - 2 )
            {
                normals[ 3 * id1 + 0 ] = n.x();
                normals[ 3 * id1 + 1 ] = n.y();
                normals[ 3 * id1 + 2 ] = n.z();
            }
        }
        break;
    }
    case kvs::LineObject::Segment:
    {
        const size_t nconnections = line->numberOfConnections();
        for ( size_t i = 0; i < nconnections; i++ )
        {
            const size_t id0 = line->connections().at( 2 * i );
            const size_t id1 = line->connections().at( 2 * i + 1 );
            const kvs::Vec3 p0 = line->coord( id0 );
            const kvs::Vec3 p1 = line->coord( id1 );
            const kvs::Vec3 n = ( p1 - p0 ).normalized();
            normals[ 3 * id0 + 0 ] = n.x();
            normals[ 3 * id0 + 1 ] = n.y();
            normals[ 3 * id0 + 2 ] = n.z();
            normals[ 3 * id1 + 0 ] = n.x();
            normals[ 3 * id1 + 1 ] = n.y();
            normals[ 3 * id1 + 2 ] = n.z();
        }
        break;
    }
    case kvs::LineObject::Polyline:
    {
        const size_t nconnections = line->numberOfConnections();
        for ( size_t i = 0; i < nconnections; i++ )
        {
            const size_t id0 = line->connections().at( 2 * i );
            const size_t id1 = line->connections().at( 2 * i + 1 );
            for ( size_t j = id0; j < id1; j++ )
            {
                const kvs::Vec3 p0 = line->coord( j );
                const kvs::Vec3 p1 = line->coord( j + 1 );
                const kvs::Vec3 n = ( p1 - p0 ).normalized();
                normals[ 3 * j + 0 ] = n.x();
                normals[ 3 * j + 1 ] = n.y();
                normals[ 3 * j + 2 ] = n.z();
                if ( j == id1 - 1 )
                {
                    normals[ 3 * ( j + 1 ) + 0 ] = n.x();
                    normals[ 3 * ( j + 1 ) + 1 ] = n.y();
                    normals[ 3 * ( j + 1 ) + 2 ] = n.z();
                }
            }
        }
        break;
    }
    case kvs::LineObject::Strip:
    {
        const size_t nvertices = line->numberOfVertices();
        for ( size_t i = 0; i < nvertices - 1; i++ )
        {
            const kvs::Vec3 p0 = line->coord( i );
            const kvs::Vec3 p1 = line->coord( i + 1 );
            const kvs::Vec3 n = ( p1 - p0 ).normalized();
            normals[ 3 * i + 0 ] = n.x();
            normals[ 3 * i + 1 ] = n.y();
            normals[ 3 * i + 2 ] = n.z();
            if ( i == nvertices - 2 )
            {
                normals[ 3 * ( i + 1 ) + 0 ] = n.x();
                normals[ 3 * ( i + 1 ) + 1 ] = n.y();
                normals[ 3 * ( i + 1 ) + 2 ] = n.z();
            }
        }
        break;
    }
    default: break;
    }

    return normals;
}

namespace DisplayList
{

GLuint Gen( GLsizei range )
{
    GLuint ret = 0;
    KVS_GL_CALL( ret = glGenLists( range ) );
    return ret;
}

void Del( GLuint list, GLsizei range )
{
    KVS_GL_CALL( glDeleteLists( list, range ) );
}

void Call( GLuint list )
{
    KVS_GL_CALL( glCallList( list ) );
}

void New( GLuint list, GLenum mode )
{
    KVS_GL_CALL( glNewList( list, mode ) );
}

void End()
{
    KVS_GL_CALL( glEndList() );
}

GLboolean IsList( GLuint list )
{
    GLboolean ret = GL_FALSE;
    KVS_GL_CALL( ret = glIsList( list ) );
    return ret;
}

bool IsValid( GLuint list )
{
    return IsList( list ) == GL_TRUE;
}

} // end of namespace DisplayList

}

namespace kvs
{

/*===========================================================================*/
/**
 *  @brief  Constructs a new StylizedLineRenderer class.
 */
/*===========================================================================*/
StylizedLineRenderer::StylizedLineRenderer():
    m_width( 0 ),
    m_height( 0 ),
    m_object( NULL ),
    m_has_connection( false ),
    m_shader( NULL ),
    m_radius_size( 0.05f ),
    m_halo_size( 0.0f )
{
    this->setShader( kvs::Shader::Lambert() );
}

/*===========================================================================*/
/**
 *  @brief  Destroys the StylizedLineRenderer class.
 */
/*===========================================================================*/
StylizedLineRenderer::~StylizedLineRenderer()
{
    if ( m_shader ) { delete m_shader; }
}

/*===========================================================================*/
/**
 *  @brief  Executes rendering process.
 *  @param  object [in] pointer to the object
 *  @param  camera [in] pointer to the camera
 *  @param  light [in] pointer to the light
 */
/*===========================================================================*/
void StylizedLineRenderer::exec( kvs::ObjectBase* object, kvs::Camera* camera, kvs::Light* light )
{
    kvs::LineObject* line = kvs::LineObject::DownCast( object );
    m_has_connection = line->numberOfConnections() > 0;

    BaseClass::startTimer();

    kvs::OpenGL::WithPushedAttrib p( GL_ALL_ATTRIB_BITS );
    kvs::OpenGL::Enable( GL_DEPTH_TEST );
    kvs::OpenGL::Enable( GL_TEXTURE_2D );
    kvs::OpenGL::Enable( GL_BLEND );
    kvs::OpenGL::SetBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    const size_t width = camera->windowWidth();
    const size_t height = camera->windowHeight();
    const bool window_created = m_width == 0 && m_height == 0;
    if ( window_created )
    {
        m_width = width;
        m_height = height;
        m_object = object;
        this->create_shader_program();
        this->create_buffer_object( line );
        this->create_shape_texture();
        this->create_diffuse_texture();
    }

    const bool window_resized = m_width != width || m_height != height;
    if ( window_resized )
    {
        m_width = width;
        m_height = height;
    }

    const bool object_changed = m_object != object;
    if ( object_changed )
    {
        m_object = object;
        m_shader_program.release();
        this->create_shader_program();
        this->create_buffer_object( line );
    }

    kvs::ProgramObject::Binder bind0( m_shader_program );
    kvs::Texture::Binder unit0( m_shape_texture, 0 );
    kvs::Texture::SetEnv( GL_TEXTURE_ENV_MODE, GL_REPLACE );
    kvs::Texture::Binder unit1( m_diffuse_texture, 1 );
    kvs::Texture::SetEnv( GL_TEXTURE_ENV_MODE, GL_REPLACE );
    {
        const kvs::Mat4 M = kvs::OpenGL::ModelViewMatrix();
        const kvs::Mat4 P = kvs::OpenGL::ProjectionMatrix();
        const kvs::Mat3 N = kvs::Mat3( M[0].xyz(), M[1].xyz(), M[2].xyz() );
        m_shader_program.setUniform( "ModelViewMatrix", M );
        m_shader_program.setUniform( "ProjectionMatrix", P );
        m_shader_program.setUniform( "NormalMatrix", N );
        m_shader_program.setUniform( "shape_texture", 0 );
        m_shader_program.setUniform( "diffuse_texture", 1 );

        ::DisplayList::Call( m_display_list_id );
    }

    BaseClass::stopTimer();
}

/*===========================================================================*/
/**
 *  @brief  Creates shader program.
 */
/*===========================================================================*/
void StylizedLineRenderer::create_shader_program()
{
    kvs::ShaderSource vert( "stylized_line.vert" );
    kvs::ShaderSource frag( "stylized_line.frag" );
    if ( isEnabledShading() )
    {
        switch ( m_shader->type() )
        {
        case kvs::Shader::LambertShading: frag.define("ENABLE_LAMBERT_SHADING"); break;
        case kvs::Shader::PhongShading: frag.define("ENABLE_PHONG_SHADING"); break;
        case kvs::Shader::BlinnPhongShading: frag.define("ENABLE_BLINN_PHONG_SHADING"); break;
        default: break; // NO SHADING
        }

        if ( kvs::OpenGL::Boolean( GL_LIGHT_MODEL_TWO_SIDE ) == GL_TRUE )
        {
            frag.define("ENABLE_TWO_SIDE_LIGHTING");
        }
    }

    m_shader_program.build( vert, frag );
    m_shader_program.bind();
    m_shader_program.setUniform( "shading.Ka", m_shader->Ka );
    m_shader_program.setUniform( "shading.Kd", m_shader->Kd );
    m_shader_program.setUniform( "shading.Ks", m_shader->Ks );
    m_shader_program.setUniform( "shading.S",  m_shader->S );
    m_shader_program.unbind();
}

/*===========================================================================*/
/**
 *  @brief  Creates buffer object.
 *  @param  line [in] pointer to the line object
 */
/*===========================================================================*/
void StylizedLineRenderer::create_buffer_object( const kvs::LineObject* line )
{
    if ( line->numberOfColors() != 1 && line->colorType() == kvs::LineObject::LineColor )
    {
        kvsMessageError("Not supported line color type.");
        return;
    }

    if ( ::DisplayList::IsValid( m_display_list_id ) ) { ::DisplayList::Del( m_display_list_id, 1 ); }

    kvs::ValueArray<kvs::Real32> coords = line->coords();
    kvs::ValueArray<kvs::UInt8> colors = ::VertexColors( line );
    kvs::ValueArray<kvs::Real32> normals = ::VertexNormals( line );

    if ( ( m_display_list_id = ::DisplayList::Gen( 1 ) ) )
    {
        switch ( line->lineType() )
        {
        case kvs::LineObject::Polyline:
        {
            ::DisplayList::New( m_display_list_id, GL_COMPILE );
            const size_t nlines = line->numberOfConnections();
            const kvs::UInt32* pconnections = line->connections().data();
            for ( size_t i = 0; i < nlines; i++ )
            {
                KVS_GL_CALL_BEG( glBegin( GL_QUAD_STRIP ) );
                const size_t index0 = *(pconnections++);
                const size_t index1 = *(pconnections++);
                for ( size_t j = index0; j <= index1; j++ )
                {
                    const float halo_size = m_halo_size;
                    const float halo_factor = 1.0f + 2.0f * halo_size;
                    const float radius = m_radius_size;
                    const float rot = 0.0f;
                    const float zdiff = 0.0f;
                    KVS_GL_CALL_VER( glColor3ubv( colors.data() + 3 * j ) );
                    KVS_GL_CALL_VER( glNormal3fv( normals.data() + 3 * j ) );
                    KVS_GL_CALL_VER( glTexCoord4f( -radius * halo_factor, radius, rot, zdiff ) );
                    KVS_GL_CALL_VER( glVertex3fv( coords.data() + 3 * j ) );
                    KVS_GL_CALL_VER( glTexCoord4f(  radius * halo_factor, radius, rot, zdiff ) );
                    KVS_GL_CALL_VER( glVertex3fv( coords.data() + 3 * j ) );
                }
                KVS_GL_CALL_END( glEnd() );
            }
            ::DisplayList::End();
            break;
        }
        case kvs::LineObject::Strip:
        {
            ::DisplayList::New( m_display_list_id, GL_COMPILE );
            KVS_GL_CALL_BEG( glBegin( GL_QUAD_STRIP ) );
            const size_t nvertices = line->numberOfVertices();
            for ( size_t i = 0; i < nvertices; i++ )
            {
                const float halo_size = m_halo_size;
                const float halo_factor = 1.0f + 2.0f * halo_size;
                const float radius = m_radius_size;
                const float rot = 0.0f;
                const float zdiff = 0.0f;
                KVS_GL_CALL_VER( glColor3ubv( colors.data() + 3 * i ) );
                KVS_GL_CALL_VER( glNormal3fv( normals.data() + 3 * i ) );
                KVS_GL_CALL_VER( glTexCoord4f( -radius * halo_factor, radius, rot, zdiff ) );
                KVS_GL_CALL_VER( glVertex3fv( coords.data() + 3 * i ) );
                KVS_GL_CALL_VER( glTexCoord4f(  radius * halo_factor, radius, rot, zdiff ) );
                KVS_GL_CALL_VER( glVertex3fv( coords.data() + 3 * i ) );
            }
            KVS_GL_CALL_END( glEnd() );
            ::DisplayList::End();
            break;
        }
        default: break;
        }
    }
}

void StylizedLineRenderer::create_shape_texture()
{
    const size_t resolution = 256;
    kvs::ValueArray<kvs::Real32> shape( resolution * resolution * 4 );
    for ( size_t j = 0, index = 0; j < resolution; j++ )
    {
        for ( size_t i = 0; i < resolution; i++, index++ )
        {
            const size_t index4 = index * 4;
            const kvs::Real32 x = ( i * 2.0f ) / kvs::Real32( resolution ) - 1.0f;

            // Cylinder shape.
            shape[ index4 + 0 ] = x * 0.5f + 0.5f;
            shape[ index4 + 1 ] = std::sqrt( 1.0f - x * x );
            shape[ index4 + 2 ] = std::sqrt( 1.0f - x * x );
        }
    }

    m_shape_texture.setWrapS( GL_REPEAT );
    m_shape_texture.setWrapT( GL_REPEAT );
    m_shape_texture.setMagFilter( GL_NEAREST );
    m_shape_texture.setMinFilter( GL_NEAREST );
    m_shape_texture.setPixelFormat( GL_RGBA32F_ARB, GL_RGBA, GL_FLOAT );
    m_shape_texture.create( resolution, resolution, shape.data() );
}

void StylizedLineRenderer::create_diffuse_texture()
{
/*
    const int resolution = 256;
    kvs::ValueArray<kvs::Real32> diffuse( resolution * resolution * 4 );
    for ( int j = 0, index = 0; j < resolution; j++ )
    {
        for ( int i = 0; i < resolution; i++, index++ )
        {
            const size_t index4 = index * 4;
//            const size_t index4 = ( j * resolution + i ) * 4;
//            const float x = kvs::Real32(i) / kvs::Real32( resolution );
            const float y = kvs::Real32(j) / kvs::Real32( resolution );

            if ( abs( ( resolution / 2 ) - i ) > ( j % ( resolution / 4 ) ) )
            {
                diffuse[ index4 + 0 ] = 1.0f;
                diffuse[ index4 + 1 ] = 1.0f;
                diffuse[ index4 + 2 ] = 1.0f;
            }
            else
            {
                diffuse[ index4 + 0 ] = 0.5f;
                diffuse[ index4 + 1 ] = 0.5f;
                diffuse[ index4 + 2 ] = 0.5f;
            }
            diffuse[ index4 + 3 ] = 1.0f;

//            diffuse[ index4 + 0 ] = (y * 4.0f) - (float)(int)(y*4.0f);
//            diffuse[ index4 + 1 ] = (y * 4.0f) - (float)(int)(y*4.0f);
//            diffuse[ index4 + 2 ] = (y * 4.0f) - (float)(int)(y*4.0f);
//            diffuse[ index4 + 3 ] = 1.0f;
        }
    }
    m_diffuse_texture.setWrapS( GL_REPEAT );
    m_diffuse_texture.setWrapT( GL_REPEAT );
    m_diffuse_texture.setMagFilter( GL_LINEAR );
    m_diffuse_texture.setMinFilter( GL_LINEAR );
    m_diffuse_texture.setPixelFormat( GL_RGBA32F, GL_RGBA, GL_FLOAT );
    m_diffuse_texture.create( resolution, resolution, diffuse.data() );
*/

    kvs::ValueArray<kvs::UInt8> diffuse( 3 );
    diffuse[0] = 255;
    diffuse[1] = 255;
    diffuse[2] = 255;

    m_diffuse_texture.setWrapS( GL_REPEAT );
    m_diffuse_texture.setWrapT( GL_REPEAT );
    m_diffuse_texture.setMagFilter( GL_LINEAR );
    m_diffuse_texture.setMinFilter( GL_LINEAR );
    m_diffuse_texture.setPixelFormat( GL_RGB, GL_RGB, GL_UNSIGNED_BYTE );
    m_diffuse_texture.create( 1, 1, diffuse.data() );
}

} // end of namespace kvs
