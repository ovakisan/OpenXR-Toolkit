// MIT License
//
// Copyright(c) 2024 OpenXR-Toolkit Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// clang-format off

// Piecewise-linear helmet overlay mapping.
//
// The display has three vertical zones (top bar, game content, bottom bar).
// The PNG has three vertical zones (sun shade, visor opening, jaw/vent).
//
// This shader stretches each PNG zone to fill the corresponding display zone:
//   Display top bar      [0,          GameRect.y]  ->  PNG sun shade   [0,            VisorRect.y]
//   Display game content [GameRect.y, GameRect.w]  ->  PNG visor       [VisorRect.y,  VisorRect.w]
//   Display bottom bar   [GameRect.w, 1]           ->  PNG jaw/vent    [VisorRect.w,  1]
//
// Hardware alpha blending composites the result: transparent visor pixels
// preserve game content, opaque sun-shade/jaw pixels fill the bars.

cbuffer config : register(b0) {
    float4 GameRect;   // x=left, y=top, z=right, w=bottom — game area in display [0..1]
    float4 VisorRect;  // x=left, y=top, z=right, w=bottom — visor area in PNG [0..1]
    float  Brightness; // 0..1 multiplier applied to helmet RGB (controls overlay brightness)
    float3 _pad;       // padding to 16-byte alignment
};

SamplerState helmetSampler : register(s0);
Texture2D   helmetTexture  : register(t0);

// Piecewise linear remap: maps display coordinate to PNG coordinate.
//   [0, gameEdge]          ->  [0, visorEdge]           (bar -> helmet frame)
//   [gameEdge, gameEdge2]  ->  [visorEdge, visorEdge2]  (game -> visor)
//   [gameEdge2, 1]         ->  [visorEdge2, 1]          (bar -> helmet frame)
float remapPiecewise(float coord, float gameEdge, float gameEdge2, float visorEdge, float visorEdge2) {
    if (coord < gameEdge) {
        // Bar region before game content
        float t = (gameEdge > 0.001) ? (coord / gameEdge) : 0.0;
        return t * visorEdge;
    } else if (coord > gameEdge2) {
        // Bar region after game content
        float barSize = 1.0 - gameEdge2;
        float t = (barSize > 0.001) ? ((coord - gameEdge2) / barSize) : 1.0;
        return visorEdge2 + t * (1.0 - visorEdge2);
    } else {
        // Game content region
        float gameSize = gameEdge2 - gameEdge;
        float t = (gameSize > 0.001) ? ((coord - gameEdge) / gameSize) : 0.5;
        return visorEdge + t * (visorEdge2 - visorEdge);
    }
}

float4 mainHelmet(in float4 position : SV_POSITION,
                  in float2 texcoord : TEXCOORD0) : SV_TARGET {
    float u = texcoord.x;
    float v = texcoord.y;

    float helmU = remapPiecewise(u, GameRect.x, GameRect.z, VisorRect.x, VisorRect.z);
    float helmV = remapPiecewise(v, GameRect.y, GameRect.w, VisorRect.y, VisorRect.w);

    float4 color = helmetTexture.Sample(helmetSampler, float2(helmU, helmV));
    color.rgb *= Brightness;
    return color;
}
