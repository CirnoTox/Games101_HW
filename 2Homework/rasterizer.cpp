// clang-format off
//
// Created by goksu on 4/6/19.
//

#include <algorithm>
#include <Eigen/Dense>
#include <cstdlib>
#include <eigen3/Eigen/src/Core/Matrix.h>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>

rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}


static bool insideTriangle(int x, int y, const Vector3f* _v)
{  
    Eigen::Vector2f checkedXY(x,y);
    auto _p0=Vector2f(_v[0][0],_v[0][1]);
    auto _p1=Vector2f(_v[1][0],_v[1][1]);
    auto _p2=Vector2f(_v[2][0],_v[2][1]);
    auto v0=_p0-_p1;
    auto v1=_p1-_p2;
    auto v2=_p2-_p0;
    auto checkV0=_p0-checkedXY;
    auto checkV1=_p1-checkedXY;
    auto checkV2=_p2-checkedXY;
    auto cross2=[](Vector2f v1,Vector2f v2){
        return (v1.x()*v2.y()-v1.y()*v2.x())>0;
    };
    auto cross2Smaller=[](Vector2f v1,Vector2f v2){
        return (v1.x()*v2.y()-v1.y()*v2.x())<0;
    };
    if(cross2(v0,checkV0)&&cross2(v1,checkV1)&&cross2(v2,checkV2)||
    cross2Smaller(v0,checkV0)&&cross2Smaller(v1,checkV1)&&cross2Smaller(v2,checkV2)){
        return true;
    }
    return false;
    // TODO : Implement this function to check if the point (x, y) is inside the triangle represented by _v[0], _v[1], _v[2]
}

// return 0 for not inside,1 for 25%, 2for50%,3for75%,4for100%
static int ssInsideTriangle(int x, int y, const Vector3f* _v,int ssPos=4)
{  
    Eigen::Vector2f checkedXY(x,y);
    auto _p0=Vector2f(_v[0][0],_v[0][1]);
    auto _p1=Vector2f(_v[1][0],_v[1][1]);
    auto _p2=Vector2f(_v[2][0],_v[2][1]);
    auto v0=_p0-_p1;
    auto v1=_p1-_p2;
    auto v2=_p2-_p0;
    auto checkV0=_p0-checkedXY;
    auto checkV1=_p1-checkedXY;
    auto checkV2=_p2-checkedXY;
    
    auto cross2=[](Vector2f v1,Vector2f v2,int ssPo){
        if(ssPo==0)
            return (v1.x()*v2.y()-v1.y()*(v2.x()+0.25))>0;
        if(ssPo==1)
            return (v1.x()*(v2.y()+0.25)-v1.y()*(v2.x()+0.25))>0;
        if(ssPo==2)
            return (v1.x()*(v2.y()+0.25)-v1.y()*(v2.x()+0.75))>0;
        if(ssPo==3)
            return (v1.x()*(v2.y()+0.75)-v1.y()*(v2.x()+0.75))>0;
        return (v1.x()*v2.y()-v1.y()*v2.x())>0;
    };
    auto cross2Smaller=[](Vector2f v1,Vector2f v2,int ssPo){
        if(ssPo==0)
            return (v1.x()*v2.y()-v1.y()*(v2.x()+0.25))<0;
        if(ssPo==1)
            return (v1.x()*(v2.y()+0.25)-v1.y()*(v2.x()+0.25))<0;
        if(ssPo==2)
            return (v1.x()*(v2.y()+0.25)-v1.y()*(v2.x()+0.75))<0;
        if(ssPo==3)
            return (v1.x()*(v2.y()+0.75)-v1.y()*(v2.x()+0.75))<0;
        return (v1.x()*v2.y()-v1.y()*v2.x())<0;
    };

    size_t count=0;
    for(auto i=0;i<ssPos;i++){
        if(cross2(v0,checkV0,i)&&cross2(v1,checkV1,i)&&cross2(v2,checkV2,i)||
    cross2Smaller(v0,checkV0,i)&&cross2Smaller(v1,checkV1,i)&&cross2Smaller(v2,checkV2,i)){
            count++;
        }
    }
    return count;
    // TODO : Implement this function to check if the point (x, y) is inside the triangle represented by _v[0], _v[1], _v[2]
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3};
}

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    auto& buf = pos_buf[pos_buffer.pos_id];
    auto& ind = ind_buf[ind_buffer.ind_id];
    auto& col = col_buf[col_buffer.col_id];

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)
    {
        Triangle t;
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        //Viewport transformation
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i)
        {
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}

//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    auto v = t.toVector4();
    int count=0;
    float min_x = std::min(v[0].x(), std::min(v[1].x(), v[2].x()));
    float max_x = std::max(v[0].x(), std::max(v[1].x(), v[2].x()));
    float min_y = std::min(v[0].y(), std::min(v[1].y(), v[2].y()));
    float max_y = std::max(v[0].y(), std::max(v[1].y(), v[2].y()));
    for(int x=min_x;x<=max_x;x++){
        for(int y=min_y;y<=max_y;y++){
            auto count=ssInsideTriangle(x,y,t.v);
            if(count!=0){
            //if(insideTriangle(x,y,t.v)){
                float min_depth = FLT_MAX;
                auto[alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
                float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                z_interpolated *= w_reciprocal;
                min_depth = std::min(min_depth, z_interpolated);
                if(depth_buf[get_index(x,y)]>min_depth){
                    depth_buf[get_index(x,y)]=min_depth;
                    Vector3f pix=Vector3f();
                    pix<<float(x),float(y),min_depth;
                    auto col=t.getColor();
                    col[0]=count*col[0]/4;
                    col[1]=count*col[1]/4;
                    col[2]=count*col[2]/4;
                    set_pixel(pix,col);
                }
                if(count!=4){
                    depth_buf[get_index(x,y)]=FLT_MAX;
                }
            }
        }
    }
    // TODO : Find out the bounding box of current triangle.
    // iterate through the pixel and find if the current pixel is inside the triangle
    
    // If so, use the following code to get the interpolated z value.
    //auto[alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
    //float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
    //float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
    //z_interpolated *= w_reciprocal;

    // TODO : set the current pixel (use the set_pixel function) to the color of the triangle (use getColor function) if it should be painted.
}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h)
{
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;

}
int rst::rasterizer::get_index_super(int x, int y,int ssPos)
{
    return ((height-1-y)*width + x)*ssPos;
}

void rst::rasterizer::set_pixel_super(const Eigen::Vector3f& point, const Eigen::Vector3f& color,int ssPos)
{
    //old index: auto ind = point.y() + point.x() * width;
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;

}




// clang-format on
