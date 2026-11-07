#pragma once
namespace a2fo::native11::shaders {
inline constexpr const char* vertex=R"hlsl(
cbuffer FrameData:register(b0) {
 row_major float4x4 world_to_clip;
 row_major float4x4 local_to_world;
 row_major float4x4 normal_to_world;
 float4 tint;
 float4 emission;
 float4 ambient;
 float4 light_pos_type[8];
 float4 light_dir_intensity[8];
 float4 light_colour_start[8];
 float4 light_range_cone[8];
 uint light_count; uint flags; uint texture_alpha; float alpha_cutoff;
 float4 uv_row0;float4 uv_row1;
 float4 fog_colour;float4 fog_range;float4 camera_position;
};
struct VIn {float3 position:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;};
struct VOut {float4 position:SV_Position;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;};
VOut main(VIn v){
 VOut o;float4 world=mul(local_to_world,float4(v.position,1));
 o.position=mul(world_to_clip,world);o.world=world.xyz/world.w;
 o.normal=normalize(mul((float3x3)normal_to_world,v.normal));o.uv=float2(dot(uv_row0.xyz,float3(v.uv,1)),dot(uv_row1.xyz,float3(v.uv,1)));return o;
}
)hlsl";
inline constexpr const char* pixel=R"hlsl(
cbuffer FrameData:register(b0) {
 row_major float4x4 world_to_clip;
 row_major float4x4 local_to_world;
 row_major float4x4 normal_to_world;
 float4 tint;
 float4 emission;
 float4 ambient;
 float4 light_pos_type[8];
 float4 light_dir_intensity[8];
 float4 light_colour_start[8];
 float4 light_range_cone[8];
 uint light_count; uint flags; uint texture_alpha; float alpha_cutoff;
 float4 uv_row0;float4 uv_row1;
 float4 fog_colour;float4 fog_range;float4 camera_position;
};
cbuffer ShadowData:register(b1) {
 row_major float4x4 shadow_to_clip[48];float4 shadow_settings;
};
Texture2D tex:register(t0);SamplerState samp:register(s0);
Texture2DArray<float> shadow_depth:register(t1);
struct PIn {float4 position:SV_Position;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;};
float compare_depth(int2 pixel,uint slice,float reference,float2 gradient,float2 uv){
 if(any(pixel<0)||any(pixel>=(int)shadow_settings.y))return 1;
 reference+=dot(gradient,(float2(pixel)+.5)/shadow_settings.y-uv);
 return reference<=shadow_depth.Load(int4(pixel,(int)slice,0))?1:0;
}
float visibility(uint light,float3 world){
 if((flags&16)!=0)return 1;
 float base=light_range_cone[light].w;if(base<.5)return 1;
 uint slice=(uint)base-1;
 if(light_pos_type[light].w>1.5 && light_pos_type[light].w<2.5){
  float3 delta=world-light_pos_type[light].xyz;float3 a=abs(delta);
  if(a.x>=a.y && a.x>=a.z)slice+=delta.x>=0?0:1;
  else if(a.y>=a.z)slice+=delta.y>=0?2:3;
  else slice+=delta.z>=0?4:5;
 }
 float4 clip_position=mul(shadow_to_clip[slice],float4(world,1));
 if(clip_position.w<=0)return 1;
 float3 ndc=clip_position.xyz/clip_position.w;
 float2 uv=ndc.xy*float2(.5,-.5)+.5;
 // Compare at the receiver plane's depth at each tap, rather than using its
 // centre depth for four different locations. This avoids sloped surfaces
 // shadowing themselves without a large, detached-shadow bias.
 float2 dx=ddx(uv),dy=ddy(uv);float zx=ddx(ndc.z),zy=ddy(ndc.z);
 float face_dx=ddx((float)slice),face_dy=ddy((float)slice);
 float determinant=dx.x*dy.y-dx.y*dy.x;float2 gradient=0;
 if(abs(determinant)>1e-12 && face_dx==0 && face_dy==0)
  gradient=float2(zx*dy.y-zy*dx.y,dx.x*zy-dy.x*zx)/determinant;
 if(!all(abs(gradient)<1e20))gradient=0;
 if(any(abs(ndc.xy)>1)||ndc.z<0||ndc.z>1)return 1;
 // Bilinear 2x2 PCF from typed depth loads also works on D3D11 providers that
 // expose R32_FLOAT loads without hardware comparison-filter support.
 float2 location=uv*shadow_settings.y-.5;int2 pixel=(int2)floor(location);
 float2 weight=frac(location);float reference=ndc.z-shadow_settings.x;
 if(shadow_settings.z>.5 && shadow_settings.z<1.5)
  return compare_depth((int2)floor(uv*shadow_settings.y),slice,reference,gradient,uv);
 if(shadow_settings.z>1.5){
  int2 centre=(int2)floor(uv*shadow_settings.y);float total=0;
  [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x)
   total+=compare_depth(centre+int2(x,y),slice,reference,gradient,uv);
  return total/9;
 }
 return lerp(lerp(compare_depth(pixel,slice,reference,gradient,uv),compare_depth(pixel+int2(1,0),slice,reference,gradient,uv),weight.x),
             lerp(compare_depth(pixel+int2(0,1),slice,reference,gradient,uv),compare_depth(pixel+int2(1,1),slice,reference,gradient,uv),weight.x),weight.y);
}
float4 main(PIn input):SV_Target{
 float4 texel=tex.Sample(samp,input.uv);
 // Generic alpha blending consumes straight colour. Additive ONE/ONE keeps
 // authored premultiplied coverage; opaque DDS metadata overrides stored A.
 if(texture_alpha==3)texel.a=1;
 else if(texture_alpha==2 && (flags&8)==0)
  texel.rgb=texel.a>1e-6?texel.rgb/texel.a:float3(0,0,0);
 float4 base=texel*tint;
 if((flags&2)!=0)clip(base.a-alpha_cutoff);
 float3 n=normalize(input.normal);float3 lighting=ambient.rgb;
 [loop]for(uint i=0;i<light_count;++i){
  float type=light_pos_type[i].w;
  float3 L;float attenuation=1;
  if(type<1.5){L=normalize(-light_dir_intensity[i].xyz);}
  else {
   float3 delta=light_pos_type[i].xyz-input.world;float dist=length(delta);
   L=delta/max(dist,1e-6);
   float start=light_colour_start[i].w,range=light_range_cone[i].x;
   attenuation=saturate(1.0-(dist-start)/range);
   if(type>2.5){
    float cone=dot(normalize(light_dir_intensity[i].xyz),-L);
    float inner=cos(radians(light_range_cone[i].y*.5));
    float outer=cos(radians(light_range_cone[i].z*.5));
    attenuation*=(inner-outer>1e-6)?saturate((cone-outer)/(inner-outer)):(cone>=outer?1.0:0.0);
   }
  }
  lighting+=light_colour_start[i].rgb*light_dir_intensity[i].w*max(dot(n,L),0)*attenuation*visibility(i,input.world);
 }
 float3 shaded=base.rgb*lighting+texel.rgb*emission.rgb;
 if(fog_range.z>.5 && (flags&32)==0){
  float amount=saturate((length(input.world-camera_position.xyz)-fog_range.x)/(fog_range.y-fog_range.x));
  shaded=lerp(shaded,fog_colour.rgb,amount);
 }
 return float4(shaded,base.a);
}
)hlsl";
inline constexpr const char* shadow_pixel=R"hlsl(
cbuffer FrameData:register(b0) {
 row_major float4x4 world_to_clip;
 row_major float4x4 local_to_world;
 row_major float4x4 normal_to_world;
 float4 tint;float4 emission;float4 ambient;
 float4 light_pos_type[8];float4 light_dir_intensity[8];
 float4 light_colour_start[8];float4 light_range_cone[8];
 uint light_count;uint flags;uint texture_alpha;float alpha_cutoff;
 float4 uv_row0;float4 uv_row1;
 float4 fog_colour;float4 fog_range;float4 camera_position;
};
Texture2D tex:register(t0);SamplerState samp:register(s0);
struct PIn {float4 position:SV_Position;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;};
void main(PIn input){
 if((flags&2)!=0){float alpha=texture_alpha==3?1:tex.Sample(samp,input.uv).a;clip(alpha*tint.a-alpha_cutoff);}
}
)hlsl";
} // namespace a2fo::native11::shaders
