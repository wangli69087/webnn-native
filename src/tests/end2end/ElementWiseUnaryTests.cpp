// Copyright 2021 The WebNN-native Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tests/WebnnTest.h"

class ElementWiseUnaryTests : public WebnnTest {
  protected:
    enum UnaryOpType {
        kAbs = 0,
        kCeil,
        kCos,
        kExp,
        kFloor,
        kLog,
        kNeg,
        kSin,
        kTan,
    };

    void CheckElementWiseUnary(UnaryOpType type,
                               const std::vector<float>& inputData,
                               const std::vector<float>& expectedValue,
                               const std::vector<int32_t>& shape) {
        const wnn::GraphBuilder builder = wnn::CreateGraphBuilder(GetContext());
        const wnn::Operand a = utils::BuildInput(builder, "a", shape);
        wnn::Operand b;
        switch (type) {
            case kAbs:
                b = builder.Abs(a);
                break;
            case kCeil:
                b = builder.Ceil(a);
                break;
            case kCos:
                b = builder.Cos(a);
                break;
            case kExp:
                b = builder.Exp(a);
                break;
            case kFloor:
                b = builder.Floor(a);
                break;
            case kLog:
                b = builder.Log(a);
                break;
            case kNeg:
                b = builder.Neg(a);
                break;
            case kSin:
                b = builder.Sin(a);
                break;
            case kTan:
                b = builder.Tan(a);
                break;
            default:
                DAWN_ASSERT(0);
        }
        const wnn::Graph graph = utils::Build(builder, {{"b", b}});
        ASSERT_TRUE(graph);
        std::vector<float> result(utils::SizeOfShape(shape));
        utils::Compute(graph, {{"a", inputData}}, {{"b", result}});
        EXPECT_TRUE(utils::CheckValue(result, expectedValue));
    }
};

TEST_F(ElementWiseUnaryTests, Abs) {
    CheckElementWiseUnary(UnaryOpType::kAbs, {-1, 0, 1}, {1, 0, 1}, {3});
    CheckElementWiseUnary(UnaryOpType::kAbs, {-1.1, 0, 1.1, 2.2, 0, -2.2},
                          {1.1, 0, 1.1, 2.2, 0, 2.2}, {2, 3});
    CheckElementWiseUnary(UnaryOpType::kAbs, {-1.1, 0, 1.1, 2.2, 0, -2.2},
                          {1.1, 0, 1.1, 2.2, 0, 2.2}, {1, 2, 3});
    CheckElementWiseUnary(UnaryOpType::kAbs, {-1.1, 0, 1.1, 2.2, 0, -2.2},
                          {1.1, 0, 1.1, 2.2, 0, 2.2}, {1, 2, 3, 1});
}

TEST_F(ElementWiseUnaryTests, Ceil) {
    CheckElementWiseUnary(UnaryOpType::kCeil, {-1.1, 0, 1.1}, {-1, 0, 2}, {3});
    CheckElementWiseUnary(UnaryOpType::kCeil, {-1.1, 0, 1.1, -2.2, 0, 2.2}, {-1, 0, 2, -2, 0, 3},
                          {2, 3});
    CheckElementWiseUnary(UnaryOpType::kCeil, {-1.1, 0, 1.1, -2.2, 0, 2.2}, {-1, 0, 2, -2, 0, 3},
                          {1, 2, 3});
    CheckElementWiseUnary(UnaryOpType::kCeil, {-1.1, 0, 1.1, -2.2, 0, 2.2}, {-1, 0, 2, -2, 0, 3},
                          {1, 2, 3, 1});
}

TEST_F(ElementWiseUnaryTests, Cos) {
    CheckElementWiseUnary(UnaryOpType::kCos, {1.4124068, 1.9740626, -0.06506752, 0.73539704},
                          {0.15772809, -0.39242464, 0.99788386, 0.74156445}, {4});
    CheckElementWiseUnary(
        UnaryOpType::kCos,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {0.15772809, -0.39242464, 0.99788386, 0.74156445, 0.84491396, 0.6231265, 0.99164057,
         0.94000137, 0.47485894, 0.7882008, 0.9769663, 0.75491464},
        {3, 4});
    CheckElementWiseUnary(
        UnaryOpType::kCos,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {0.15772809, -0.39242464, 0.99788386, 0.74156445, 0.84491396, 0.6231265, 0.99164057,
         0.94000137, 0.47485894, 0.7882008, 0.9769663, 0.75491464},
        {3, 2, 2});
    CheckElementWiseUnary(
        UnaryOpType::kCos,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {0.15772809, -0.39242464, 0.99788386, 0.74156445, 0.84491396, 0.6231265, 0.99164057,
         0.94000137, 0.47485894, 0.7882008, 0.9769663, 0.75491464},
        {3, 2, 2, 1});
}

TEST_F(ElementWiseUnaryTests, Exp) {
    CheckElementWiseUnary(UnaryOpType::kExp, {-1, 0, 1}, {0.36787945, 1., 2.71828175}, {3});
    CheckElementWiseUnary(UnaryOpType::kExp,
                          {0.3143407, 0.03632548, 0.5354084, -0.5000897, 1.2028517, -1.2581364,
                           -1.5108215, -1.2340564, 1.3860914, -0.2944251, -1.5065757, -0.4673513},
                          {1.3693562, 1.0369933, 1.7081455, 0.60647625, 3.329598, 0.28418314,
                           0.22072858, 0.29110932, 3.999188, 0.7449597, 0.22166772, 0.6266599},
                          {3, 4});
    CheckElementWiseUnary(
        UnaryOpType::kExp,
        {0.3143407,   0.03632548,  0.5354084,  -0.5000897,  1.2028517,   -1.2581364,  -1.5108215,
         -1.2340564,  1.3860914,   -0.2944251, -1.5065757,  -0.4673513,  0.56616277,  0.77866685,
         -0.01097398, 1.0758846,   0.6035437,  0.36806744,  0.03906458,  -0.54385495, 0.10609569,
         -0.40644982, -1.2890846,  1.3825086,  0.51489764,  1.6407244,   -0.67886734, -0.6556329,
         1.0399923,   0.1484657,   1.011217,   0.8451463,   0.75473833,  -2.0161264,  1.6406634,
         -0.01692923, -0.7986609,  0.97758174, 0.893054,    -0.01632686, -1.9721986,  -0.75843745,
         0.42327842,  -0.08648382, -1.3960054, 0.7547995,   -0.42002508, -1.784105,   1.0171342,
         0.3634587,   0.4158588,   -1.0103701, -0.23202766, 0.6390487,   -0.22796124, 0.11259284,
         0.3690759,   -0.18703128, 0.07711394, 2.9116163},
        {1.3693562,  1.0369933, 1.7081455,  0.60647625, 3.329598,  0.28418314, 0.22072858,
         0.29110932, 3.999188,  0.7449597,  0.22166772, 0.6266599, 1.7614948,  2.1785662,
         0.98908603, 2.9325857, 1.8285872,  1.4449395,  1.0398376, 0.58050615, 1.1119282,
         0.6660105,  0.2755229, 3.984886,   1.6734672,  5.1589055, 0.5071911,  0.5191134,
         2.829195,   1.160053,  2.7489443,  2.3283184,  2.127055,  0.13317032, 5.1585903,
         0.9832132,  0.4499311, 2.6580205,  2.442578,   0.9838057, 0.13915059, 0.46839777,
         1.5269594,  0.9171504, 0.24758399, 2.1271849,  0.6570303, 0.1679473,  2.7652586,
         1.4382954,  1.5156718, 0.36408418, 0.79292417, 1.8946776, 0.79615515, 1.1191761,
         1.4463972,  0.8294177, 1.0801651,  18.386492},
        {3, 4, 5});
    CheckElementWiseUnary(
        UnaryOpType::kExp,
        {0.3143407,   0.03632548,  0.5354084,  -0.5000897,  1.2028517,   -1.2581364,  -1.5108215,
         -1.2340564,  1.3860914,   -0.2944251, -1.5065757,  -0.4673513,  0.56616277,  0.77866685,
         -0.01097398, 1.0758846,   0.6035437,  0.36806744,  0.03906458,  -0.54385495, 0.10609569,
         -0.40644982, -1.2890846,  1.3825086,  0.51489764,  1.6407244,   -0.67886734, -0.6556329,
         1.0399923,   0.1484657,   1.011217,   0.8451463,   0.75473833,  -2.0161264,  1.6406634,
         -0.01692923, -0.7986609,  0.97758174, 0.893054,    -0.01632686, -1.9721986,  -0.75843745,
         0.42327842,  -0.08648382, -1.3960054, 0.7547995,   -0.42002508, -1.784105,   1.0171342,
         0.3634587,   0.4158588,   -1.0103701, -0.23202766, 0.6390487,   -0.22796124, 0.11259284,
         0.3690759,   -0.18703128, 0.07711394, 2.9116163},
        {1.3693562,  1.0369933, 1.7081455,  0.60647625, 3.329598,  0.28418314, 0.22072858,
         0.29110932, 3.999188,  0.7449597,  0.22166772, 0.6266599, 1.7614948,  2.1785662,
         0.98908603, 2.9325857, 1.8285872,  1.4449395,  1.0398376, 0.58050615, 1.1119282,
         0.6660105,  0.2755229, 3.984886,   1.6734672,  5.1589055, 0.5071911,  0.5191134,
         2.829195,   1.160053,  2.7489443,  2.3283184,  2.127055,  0.13317032, 5.1585903,
         0.9832132,  0.4499311, 2.6580205,  2.442578,   0.9838057, 0.13915059, 0.46839777,
         1.5269594,  0.9171504, 0.24758399, 2.1271849,  0.6570303, 0.1679473,  2.7652586,
         1.4382954,  1.5156718, 0.36408418, 0.79292417, 1.8946776, 0.79615515, 1.1191761,
         1.4463972,  0.8294177, 1.0801651,  18.386492},
        {3, 2, 2, 5});
}

TEST_F(ElementWiseUnaryTests, Floor) {
    CheckElementWiseUnary(UnaryOpType::kFloor, {-1.1, 0, 1.1}, {-2, 0, 1}, {3});
    CheckElementWiseUnary(UnaryOpType::kFloor, {-1.1, 0, 1.1, -2.2, 0, 2.2}, {-2, 0, 1, -3, 0, 2},
                          {2, 3});
    CheckElementWiseUnary(UnaryOpType::kFloor, {-1.1, 0, 1.1, -2.2, 0, 2.2}, {-2, 0, 1, -3, 0, 2},
                          {1, 2, 3});
    CheckElementWiseUnary(UnaryOpType::kFloor, {-1.1, 0, 1.1, -2.2, 0, 2.2}, {-2, 0, 1, -3, 0, 2},
                          {1, 2, 3, 1});
}

TEST_F(ElementWiseUnaryTests, Log) {
    CheckElementWiseUnary(UnaryOpType::kLog, {1.4599811, 0.34325936, 1.0420732},
                          {0.37842348, -1.069269, 0.04121224}, {3});
    CheckElementWiseUnary(UnaryOpType::kLog,
                          {1.4599811, 0.34325936, 1.0420732, 0.10867598, 0.39999306, 0.03704359,
                           1.5873954, 0.44784936, 0.69070333, 1.8561625, 1.4088289, 0.06367786},
                          {0.37842348, -1.069269, 0.04121224, -2.2193844, -0.91630805, -3.29566,
                           0.4620946, -0.80329835, -0.37004486, 0.6185112, 0.34275877, -2.7539184},
                          {3, 4});
    CheckElementWiseUnary(
        UnaryOpType::kLog,
        {1.4599811,  0.34325936, 1.0420732,  0.10867598, 0.39999306, 0.03704359, 1.5873954,
         0.44784936, 0.69070333, 1.8561625,  1.4088289,  0.06367786, 0.32938832, 1.2429568,
         1.1544572,  0.47578564, 1.868428,   1.2279319,  1.0712656,  1.17982,    1.460244,
         0.62389,    0.79644215, 0.4196875,  0.372386,   1.8887448,  1.4791015,  0.98091763,
         0.45482925, 0.50871295, 0.11605832, 0.86883324, 0.6235918,  1.392687,   0.75550365,
         0.35920736, 0.04935746, 0.13449927, 1.3587855,  0.9073937,  1.0731584,  1.7933426,
         1.9806778,  0.43379396, 1.3261564,  0.52664477, 0.041302,   1.5167572,  0.6400343,
         0.7669278,  1.1766342,  1.6620969,  1.2579637,  1.7453014,  0.5470841,  1.5960937,
         0.37127188, 1.9055833,  1.3749765,  0.43101534},
        {0.37842348,  -1.069269,   0.04121224, -2.2193844,  -0.91630805, -3.29566,    0.4620946,
         -0.80329835, -0.37004486, 0.6185112,  0.34275877,  -2.7539184,  -1.110518,   0.21749303,
         0.1436303,   -0.74278784, 0.62509745, 0.20533134,  0.06884073,  0.16536184,  0.3786036,
         -0.47178125, -0.22760078, -0.8682449, -0.9878243,  0.6359125,   0.39143485,  -0.01926679,
         -0.7878332,  -0.6758714,  -2.1536624, -0.14060406, -0.4722593,  0.33123496,  -0.28037068,
         -1.0238554,  -3.0086665,  -2.0061965, 0.3065913,   -0.09717887, 0.07060606,  0.58408123,
         0.68343914,  -0.83518565, 0.28228483, -0.64122903, -3.1868443,  0.4165747,   -0.44623348,
         -0.26536265, 0.16265798,  0.50807995, 0.22949426,  0.55692726,  -0.60315275, 0.46755916,
         -0.99082065, 0.64478815,  0.31843665, -0.8416116},
        {3, 4, 5});
    CheckElementWiseUnary(
        UnaryOpType::kLog,
        {1.4599811,  0.34325936, 1.0420732,  0.10867598, 0.39999306, 0.03704359, 1.5873954,
         0.44784936, 0.69070333, 1.8561625,  1.4088289,  0.06367786, 0.32938832, 1.2429568,
         1.1544572,  0.47578564, 1.868428,   1.2279319,  1.0712656,  1.17982,    1.460244,
         0.62389,    0.79644215, 0.4196875,  0.372386,   1.8887448,  1.4791015,  0.98091763,
         0.45482925, 0.50871295, 0.11605832, 0.86883324, 0.6235918,  1.392687,   0.75550365,
         0.35920736, 0.04935746, 0.13449927, 1.3587855,  0.9073937,  1.0731584,  1.7933426,
         1.9806778,  0.43379396, 1.3261564,  0.52664477, 0.041302,   1.5167572,  0.6400343,
         0.7669278,  1.1766342,  1.6620969,  1.2579637,  1.7453014,  0.5470841,  1.5960937,
         0.37127188, 1.9055833,  1.3749765,  0.43101534},
        {0.37842348,  -1.069269,   0.04121224, -2.2193844,  -0.91630805, -3.29566,    0.4620946,
         -0.80329835, -0.37004486, 0.6185112,  0.34275877,  -2.7539184,  -1.110518,   0.21749303,
         0.1436303,   -0.74278784, 0.62509745, 0.20533134,  0.06884073,  0.16536184,  0.3786036,
         -0.47178125, -0.22760078, -0.8682449, -0.9878243,  0.6359125,   0.39143485,  -0.01926679,
         -0.7878332,  -0.6758714,  -2.1536624, -0.14060406, -0.4722593,  0.33123496,  -0.28037068,
         -1.0238554,  -3.0086665,  -2.0061965, 0.3065913,   -0.09717887, 0.07060606,  0.58408123,
         0.68343914,  -0.83518565, 0.28228483, -0.64122903, -3.1868443,  0.4165747,   -0.44623348,
         -0.26536265, 0.16265798,  0.50807995, 0.22949426,  0.55692726,  -0.60315275, 0.46755916,
         -0.99082065, 0.64478815,  0.31843665, -0.8416116},
        {3, 2, 2, 5});
}

TEST_F(ElementWiseUnaryTests, Neg) {
    CheckElementWiseUnary(UnaryOpType::kNeg, {-1.1, 0, 1.1}, {1.1, 0, -1.1}, {3});
    CheckElementWiseUnary(UnaryOpType::kNeg, {-1, 0, 1.1, -2.2, 0, 2}, {1, 0, -1.1, 2.2, 0, -2},
                          {2, 3});
    CheckElementWiseUnary(UnaryOpType::kNeg, {-1, 0, 1.1, -2.2, 0, 2}, {1, 0, -1.1, 2.2, 0, -2},
                          {1, 2, 3});
    CheckElementWiseUnary(UnaryOpType::kNeg, {-1, 0, 1.1, -2.2, 0, 2}, {1, 0, -1.1, 2.2, 0, -2},
                          {1, 2, 3, 1});
}

TEST_F(ElementWiseUnaryTests, Sin) {
    CheckElementWiseUnary(UnaryOpType::kSin, {1.4124068, 1.9740626, -0.06506752, 0.73539704},
                          {0.98748258, 0.91978414, -0.06502162, 0.67088164}, {4});
    CheckElementWiseUnary(
        UnaryOpType::kSin,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {0.98748258, 0.91978414, -0.06502162, 0.67088164, -0.53490223, 0.78212105, 0.12903071,
         -0.34117074, -0.88006193, 0.61541814, 0.21339342, -0.65582306},
        {3, 4});
    CheckElementWiseUnary(
        UnaryOpType::kSin,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {0.98748258, 0.91978414, -0.06502162, 0.67088164, -0.53490223, 0.78212105, 0.12903071,
         -0.34117074, -0.88006193, 0.61541814, 0.21339342, -0.65582306},
        {3, 2, 2});
    CheckElementWiseUnary(
        UnaryOpType::kSin,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {0.98748258, 0.91978414, -0.06502162, 0.67088164, -0.53490223, 0.78212105, 0.12903071,
         -0.34117074, -0.88006193, 0.61541814, 0.21339342, -0.65582306},
        {3, 2, 2, 1});
}

TEST_F(ElementWiseUnaryTests, Tan) {
    CheckElementWiseUnary(UnaryOpType::kTan, {1.4124068, 1.9740626, -0.06506752, 0.73539704},
                          {6.26066374, -2.34384875, -0.0651595, 0.9046842}, {4});
    CheckElementWiseUnary(
        UnaryOpType::kTan,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {6.26066374, -2.34384875, -0.0651595, 0.9046842, -0.63308486, 1.2551561, 0.13011843,
         -0.36294707, -1.85331238, 0.78078852, 0.21842454, -0.86873803},
        {3, 4});
    CheckElementWiseUnary(
        UnaryOpType::kTan,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {6.26066374, -2.34384875, -0.0651595, 0.9046842, -0.63308486, 1.2551561, 0.13011843,
         -0.36294707, -1.85331238, 0.78078852, 0.21842454, -0.86873803},
        {3, 2, 2});
    CheckElementWiseUnary(
        UnaryOpType::kTan,
        {1.4124068, 1.9740626, -0.06506752, 0.73539704, -0.56439203, 0.89806247, 0.12939146,
         -0.34816208, -1.0759926, 0.66291636, 0.21504708, -0.71527237},
        {6.26066374, -2.34384875, -0.0651595, 0.9046842, -0.63308486, 1.2551561, 0.13011843,
         -0.36294707, -1.85331238, 0.78078852, 0.21842454, -0.86873803},
        {3, 2, 2, 1});
}