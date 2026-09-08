// ============================================================
// ObjLoader 单元测试 — OBJ 解析（纯逻辑无 GL）
// 覆盖：三角形 / 四边形三角化 / v//vn / 无法线自动计算 / 负索引 / 坏文件
// ============================================================
#include <gtest/gtest.h>
#include "Render/ObjLoader.h"
#include <sstream>

TEST(ObjLoaderTest, Parse_SingleTriangleWithNormals_ThreeVertices)
{
    std::istringstream obj(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\n"
        "f 1//1 2//1 3//1\n");
    std::vector<Vertex> out;
    ASSERT_TRUE(ObjLoader::parse(obj, out));
    ASSERT_EQ(out.size(), 3u);
    EXPECT_FLOAT_EQ(out[1].px, 1.0f);
    EXPECT_FLOAT_EQ(out[0].nz, 1.0f); // 法线来自 vn
}

TEST(ObjLoaderTest, Parse_Quad_TriangulatedToSixVertices)
{
    std::istringstream obj(
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "f 1 2 3 4\n"); // 四边形 → 扇形拆 2 三角形
    std::vector<Vertex> out;
    ASSERT_TRUE(ObjLoader::parse(obj, out));
    EXPECT_EQ(out.size(), 6u);
}

TEST(ObjLoaderTest, Parse_NoNormals_FaceNormalComputed)
{
    std::istringstream obj(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f 1 2 3\n"); // 无 vn → 面法线 = +Z（逆时针）
    std::vector<Vertex> out;
    ASSERT_TRUE(ObjLoader::parse(obj, out));
    EXPECT_NEAR(out[0].nz, 1.0f, 1e-5f);
}

TEST(ObjLoaderTest, Parse_FullFormatWithUv_UvCarried)
{
    std::istringstream obj(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n");
    std::vector<Vertex> out;
    ASSERT_TRUE(ObjLoader::parse(obj, out));
    EXPECT_FLOAT_EQ(out[1].u, 1.0f);
    EXPECT_FLOAT_EQ(out[2].v, 1.0f);
}

TEST(ObjLoaderTest, Parse_NegativeIndices_ResolveFromEnd)
{
    std::istringstream obj(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f -3 -2 -1\n"); // 负索引 = 从尾部倒数（合法 OBJ）
    std::vector<Vertex> out;
    ASSERT_TRUE(ObjLoader::parse(obj, out));
    ASSERT_EQ(out.size(), 3u);
    EXPECT_FLOAT_EQ(out[2].py, 1.0f);
}

TEST(ObjLoaderTest, Parse_NoFaces_ReturnsFalse)
{
    std::istringstream obj("v 0 0 0\nv 1 0 0\n# comment only\n");
    std::vector<Vertex> out;
    EXPECT_FALSE(ObjLoader::parse(obj, out)); // 没有面 = 无效模型
}

TEST(ObjLoaderTest, LoadFile_MissingFile_ReturnsFalse)
{
    std::vector<Vertex> out;
    EXPECT_FALSE(ObjLoader::loadFile("Z:/no/such/model.obj", out));
}
