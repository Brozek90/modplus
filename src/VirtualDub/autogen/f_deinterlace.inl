// Pixel shader data auto-generated by Asuka from f_deinterlace.vdshaders. DO NOT EDIT!

static const uint32 kVDFilterDeinterlaceFP_ELA1[]={
	//     ps_2_0
	//     def c0, 2, -1, -2, 1
	//     def c1, 0, 0.125490203, 0.501960814, 0
	//     dcl_pp t0
	//     dcl_pp t1
	//     dcl_pp t2
	//     dcl_pp t3
	//     dcl_pp t4
	//     dcl_pp t5
	//     dcl_pp t6.xy
	//     dcl_pp t7.xy
	//     dcl_2d s0
	//     mov_pp r0.xy, t0.wzyx
	//     mov_pp r1.xy, t1.wzyx
	//     mov_pp r2.xy, t2.wzyx
	//     mov_pp r3.xy, t3.wzyx
	//     mov_pp r4.xy, t4.wzyx
	//     texld_pp r0, r0, s0
	//     texld_pp r5, t4, s0
	//     texld_pp r1, r1, s0
	//     texld_pp r6, t5, s0
	//     texld_pp r7, t6, s0
	//     texld_pp r2, r2, s0
	//     texld_pp r8, t3, s0
	//     texld_pp r3, r3, s0
	//     texld_pp r9, t0, s0
	//     texld_pp r4, r4, s0
	//     add_pp r0.xyz, -r0, r5
	//     abs_pp r0.xyz, r0
	//     add_pp r10.xyz, -r1, r6
	//     abs_pp r10.xyz, r10
	//     mad_pp r0.xyz, r10, c0.x, r0
	//     add_pp r7.xyz, r7, -r2
	//     abs_pp r7.xyz, r7
	//     add_pp r0.xyz, r0, r7
	//     add_pp r7.xyz, r5, -r2
	//     abs_pp r7.xyz, r7
	//     add_pp r1.xyz, -r1, r8
	//     abs_pp r1.xyz, r1
	//     mad_pp r1.xyz, r7, c0.x, r1
	//     add_pp r6.xyz, r6, -r3
	//     abs_pp r6.xyz, r6
	//     add_pp r1.xyz, r1, r6
	//     add r0.xyz, r0, -r1
	//     cmp_pp r0.xyz, r0, c0.w, c0.x
	//     add_pp r6.xyz, r9, -r4
	//     abs_pp r6.xyz, r6
	//     mov_pp r7.xy, t5.wzyx
	//     texld_pp r7, r7, s0
	//     texld_pp r9, t1, s0
	//     add_pp r10.xyz, -r7, r9
	//     add_pp r7.xyz, r8, -r7
	//     add_pp r8.xyz, r8, -r3
	//     add_pp r3.xyz, -r3, r9
	//     abs_pp r3.xyz, r3
	//     abs_pp r8.xyz, r8
	//     abs_pp r7.xyz, r7
	//     abs_pp r9.xyz, r10
	//     mad_pp r6.xyz, r9, c0.x, r6
	//     texld_pp r9, t7, s0
	//     texld_pp r10, t2, s0
	//     add_pp r9.xyz, -r9, r10
	//     abs_pp r9.xyz, r9
	//     add_pp r6.xyz, r6, r9
	//     add_pp r9.xyz, -r4, r10
	//     add_pp r4.xyz, r5, -r4
	//     abs_pp r4.xyz, r4
	//     add_pp r2.xyz, -r2, r10
	//     abs_pp r2.xyz, r2
	//     mad_pp r2.xyz, r8, c0.x, r2
	//     add_pp r2.xyz, r4, r2
	//     abs_pp r4.xyz, r9
	//     mad_pp r3.xyz, r4, c0.x, r3
	//     add_pp r3.xyz, r7, r3
	//     add r4.xyz, r6, -r3
	//     cmp_pp r4.xyz, r4, c0.y, c0.z
	//     add_pp r5.xyz, -r1, r3
	//     min_pp r6.xyz, r1, r3
	//     add r1.xyz, -r2, r6
	//     cmp_pp r0.xyz, r5, r0, r4
	//     cmp_pp r0.xyz, r1, c1.x, r0
	//     mad_pp r0.xyz, r0, c1.y, c1.z
	//     mov_pp r0.w, c1.x
	//     mov_pp oC0, r0
	// 
	// // approximately 72 instruction slots used (14 texture, 58 arithmetic)
	// 
	0xffff0200,0x05000051,0xa00f0000,0x40000000,0xbf800000,0xc0000000,0x3f800000,0x05000051,
	0xa00f0001,0x00000000,0x3e008081,0x3f008081,0x00000000,0x0200001f,0x80000000,0xb02f0000,
	0x0200001f,0x80000000,0xb02f0001,0x0200001f,0x80000000,0xb02f0002,0x0200001f,0x80000000,
	0xb02f0003,0x0200001f,0x80000000,0xb02f0004,0x0200001f,0x80000000,0xb02f0005,0x0200001f,
	0x80000000,0xb0230006,0x0200001f,0x80000000,0xb0230007,0x0200001f,0x90000000,0xa00f0800,
	0x02000001,0x80230000,0xb01b0000,0x02000001,0x80230001,0xb01b0001,0x02000001,0x80230002,
	0xb01b0002,0x02000001,0x80230003,0xb01b0003,0x02000001,0x80230004,0xb01b0004,0x03000042,
	0x802f0000,0x80e40000,0xa0e40800,0x03000042,0x802f0005,0xb0e40004,0xa0e40800,0x03000042,
	0x802f0001,0x80e40001,0xa0e40800,0x03000042,0x802f0006,0xb0e40005,0xa0e40800,0x03000042,
	0x802f0007,0xb0e40006,0xa0e40800,0x03000042,0x802f0002,0x80e40002,0xa0e40800,0x03000042,
	0x802f0008,0xb0e40003,0xa0e40800,0x03000042,0x802f0003,0x80e40003,0xa0e40800,0x03000042,
	0x802f0009,0xb0e40000,0xa0e40800,0x03000042,0x802f0004,0x80e40004,0xa0e40800,0x03000002,
	0x80270000,0x81e40000,0x80e40005,0x02000023,0x80270000,0x80e40000,0x03000002,0x8027000a,
	0x81e40001,0x80e40006,0x02000023,0x8027000a,0x80e4000a,0x04000004,0x80270000,0x80e4000a,
	0xa0000000,0x80e40000,0x03000002,0x80270007,0x80e40007,0x81e40002,0x02000023,0x80270007,
	0x80e40007,0x03000002,0x80270000,0x80e40000,0x80e40007,0x03000002,0x80270007,0x80e40005,
	0x81e40002,0x02000023,0x80270007,0x80e40007,0x03000002,0x80270001,0x81e40001,0x80e40008,
	0x02000023,0x80270001,0x80e40001,0x04000004,0x80270001,0x80e40007,0xa0000000,0x80e40001,
	0x03000002,0x80270006,0x80e40006,0x81e40003,0x02000023,0x80270006,0x80e40006,0x03000002,
	0x80270001,0x80e40001,0x80e40006,0x03000002,0x80070000,0x80e40000,0x81e40001,0x04000058,
	0x80270000,0x80e40000,0xa0ff0000,0xa0000000,0x03000002,0x80270006,0x80e40009,0x81e40004,
	0x02000023,0x80270006,0x80e40006,0x02000001,0x80230007,0xb01b0005,0x03000042,0x802f0007,
	0x80e40007,0xa0e40800,0x03000042,0x802f0009,0xb0e40001,0xa0e40800,0x03000002,0x8027000a,
	0x81e40007,0x80e40009,0x03000002,0x80270007,0x80e40008,0x81e40007,0x03000002,0x80270008,
	0x80e40008,0x81e40003,0x03000002,0x80270003,0x81e40003,0x80e40009,0x02000023,0x80270003,
	0x80e40003,0x02000023,0x80270008,0x80e40008,0x02000023,0x80270007,0x80e40007,0x02000023,
	0x80270009,0x80e4000a,0x04000004,0x80270006,0x80e40009,0xa0000000,0x80e40006,0x03000042,
	0x802f0009,0xb0e40007,0xa0e40800,0x03000042,0x802f000a,0xb0e40002,0xa0e40800,0x03000002,
	0x80270009,0x81e40009,0x80e4000a,0x02000023,0x80270009,0x80e40009,0x03000002,0x80270006,
	0x80e40006,0x80e40009,0x03000002,0x80270009,0x81e40004,0x80e4000a,0x03000002,0x80270004,
	0x80e40005,0x81e40004,0x02000023,0x80270004,0x80e40004,0x03000002,0x80270002,0x81e40002,
	0x80e4000a,0x02000023,0x80270002,0x80e40002,0x04000004,0x80270002,0x80e40008,0xa0000000,
	0x80e40002,0x03000002,0x80270002,0x80e40004,0x80e40002,0x02000023,0x80270004,0x80e40009,
	0x04000004,0x80270003,0x80e40004,0xa0000000,0x80e40003,0x03000002,0x80270003,0x80e40007,
	0x80e40003,0x03000002,0x80070004,0x80e40006,0x81e40003,0x04000058,0x80270004,0x80e40004,
	0xa0550000,0xa0aa0000,0x03000002,0x80270005,0x81e40001,0x80e40003,0x0300000a,0x80270006,
	0x80e40001,0x80e40003,0x03000002,0x80070001,0x81e40002,0x80e40006,0x04000058,0x80270000,
	0x80e40005,0x80e40000,0x80e40004,0x04000058,0x80270000,0x80e40001,0xa0000001,0x80e40000,
	0x04000004,0x80270000,0x80e40000,0xa0550001,0xa0aa0001,0x02000001,0x80280000,0xa0000001,
	0x02000001,0x802f0800,0x80e40000,0x0000ffff,
};

static const uint32 kVDFilterDeinterlaceFP_ELA2[]={
	//     ps_2_0
	//     def c1, -0.501960814, 7.96875, 0, 0.5
	//     dcl_pp t0.xy
	//     dcl_pp t1.xy
	//     dcl_pp t2.xy
	//     dcl_2d s0
	//     dcl_2d s1
	//     texld_pp r0, t0, s0
	//     add r0.xyz, r0, c1.x
	//     mul r0.xyz, r0, c0.x
	//     mov_pp r0.w, c1.z
	//     mad_pp r1.x, r0.x, c1.y, t1.x
	//     add_pp r1.y, r0.w, t1.y
	//     mad_pp r2.x, r0.x, -c1.y, t2.x
	//     add_pp r2.y, -r0.w, t2.y
	//     mad_pp r3.x, r0.y, c1.y, t1.x
	//     add_pp r3.y, r0.w, t1.y
	//     mad_pp r0.x, r0.y, -c1.y, t2.x
	//     add_pp r0.y, -r0.w, t2.y
	//     mad_pp r4.x, r0.z, c1.y, t1.x
	//     add_pp r4.y, r0.w, t1.y
	//     mad_pp r5.x, r0.z, -c1.y, t2.x
	//     add_pp r5.y, -r0.w, t2.y
	//     texld_pp r1, r1, s1
	//     texld_pp r2, r2, s1
	//     texld_pp r3, r3, s1
	//     texld_pp r0, r0, s1
	//     texld_pp r5, r5, s1
	//     texld_pp r4, r4, s1
	//     add_pp r1.x, r1.x, r2.x
	//     add_pp r1.y, r3.y, r0.y
	//     add_pp r1.z, r5.z, r4.z
	//     mul_pp r0.xyz, r1, c1.w
	//     mov_pp r0.w, c1.z
	//     mov_pp oC0, r0
	// 
	// // approximately 28 instruction slots used (7 texture, 21 arithmetic)
	// 
	0xffff0200,0x05000051,0xa00f0001,0xbf008081,0x40ff0000,0x00000000,0x3f000000,0x0200001f,
	0x80000000,0xb0230000,0x0200001f,0x80000000,0xb0230001,0x0200001f,0x80000000,0xb0230002,
	0x0200001f,0x90000000,0xa00f0800,0x0200001f,0x90000000,0xa00f0801,0x03000042,0x802f0000,
	0xb0e40000,0xa0e40800,0x03000002,0x80070000,0x80e40000,0xa0000001,0x03000005,0x80070000,
	0x80e40000,0xa0000000,0x02000001,0x80280000,0xa0aa0001,0x04000004,0x80210001,0x80000000,
	0xa0550001,0xb0000001,0x03000002,0x80220001,0x80ff0000,0xb0550001,0x04000004,0x80210002,
	0x80000000,0xa1550001,0xb0000002,0x03000002,0x80220002,0x81ff0000,0xb0550002,0x04000004,
	0x80210003,0x80550000,0xa0550001,0xb0000001,0x03000002,0x80220003,0x80ff0000,0xb0550001,
	0x04000004,0x80210000,0x80550000,0xa1550001,0xb0000002,0x03000002,0x80220000,0x81ff0000,
	0xb0550002,0x04000004,0x80210004,0x80aa0000,0xa0550001,0xb0000001,0x03000002,0x80220004,
	0x80ff0000,0xb0550001,0x04000004,0x80210005,0x80aa0000,0xa1550001,0xb0000002,0x03000002,
	0x80220005,0x81ff0000,0xb0550002,0x03000042,0x802f0001,0x80e40001,0xa0e40801,0x03000042,
	0x802f0002,0x80e40002,0xa0e40801,0x03000042,0x802f0003,0x80e40003,0xa0e40801,0x03000042,
	0x802f0000,0x80e40000,0xa0e40801,0x03000042,0x802f0005,0x80e40005,0xa0e40801,0x03000042,
	0x802f0004,0x80e40004,0xa0e40801,0x03000002,0x80210001,0x80000001,0x80000002,0x03000002,
	0x80220001,0x80550003,0x80550000,0x03000002,0x80240001,0x80aa0005,0x80aa0004,0x03000005,
	0x80270000,0x80e40001,0xa0ff0001,0x02000001,0x80280000,0xa0aa0001,0x02000001,0x802f0800,
	0x80e40000,0x0000ffff,
};

static const uint32 kVDFilterDeinterlaceFP_Final[]={
	//     ps_2_0
	//     def c0, 0.5, -0.5, 0, 0
	//     dcl_pp t0.xy
	//     dcl_pp t1.xy
	//     dcl_pp t2.xy
	//     dcl_pp t3.xy
	//     dcl_pp t4.xy
	//     dcl_pp t5.xy
	//     dcl_pp t6.x
	//     dcl_2d s0
	//     dcl_2d s1
	//     dcl_2d s2
	//     dcl_2d s3
	//     dcl_2d s4
	//     dcl_2d s5
	//     texld_pp r0, t1, s0
	//     texld_pp r1, t1, s2
	//     texld_pp r2, t3, s0
	//     texld_pp r3, t3, s2
	//     texld_pp r4, t1, s4
	//     texld_pp r5, t3, s4
	//     texld_pp r6, t2, s1
	//     texld_pp r7, t2, s3
	//     texld_pp r8, t0, s1
	//     texld_pp r9, t0, s3
	//     texld_pp r10, t4, s1
	//     add_pp r0.xyz, r0, -r1
	//     abs_pp r0.xyz, r0
	//     add_pp r2.xyz, r2, -r3
	//     abs_pp r2.xyz, r2
	//     add_pp r0.xyz, r0, r2
	//     add_pp r2.xyz, r1, -r4
	//     abs_pp r2.xyz, r2
	//     add_pp r4.xyz, r3, -r5
	//     abs_pp r4.xyz, r4
	//     add_pp r2.xyz, r2, r4
	//     max_pp r4.xyz, r0, r2
	//     add_pp r0.xyz, r6, -r7
	//     add_pp r2.xyz, r6, r7
	//     abs_pp r0.xyz, r0
	//     max_pp r5.xyz, r0, r4
	//     add_pp r0.xyz, r8, r9
	//     mad_pp r0.xyz, r0, -c0.x, r1
	//     mad_pp r1.xyz, r2, -c0.x, r1
	//     texld_pp r4, t4, s3
	//     texld_pp r6, t5, s5
	//     texld_pp r7, t2, s2
	//     add_pp r4.xyz, r10, r4
	//     mad_pp r4.xyz, r4, -c0.x, r3
	//     mad_pp r3.xyz, r2, -c0.x, r3
	//     max_pp r8.xyz, r0, r4
	//     min_pp r9.xyz, r4, r0
	//     min_pp r0.xyz, r1, r3
	//     max_pp r4.xyz, r3, r1
	//     max_pp r1.xyz, r4, r9
	//     min_pp r3.xyz, r8, r0
	//     max_pp r0.xyz, r5, r3
	//     max_pp r3.xyz, r0, -r1
	//     mad_pp r0.xyz, r2, c0.x, -r3
	//     mad_pp r1.xyz, r2, c0.x, r3
	//     max_pp r2.xyz, r0, r6
	//     min_pp r0.xyz, r2, r1
	//     frc_pp r0.w, t6.x
	//     add r0.w, r0.w, c0.y
	//     cmp_pp r0.xyz, r0.w, r0, r7
	//     mov_pp r0.w, c0.z
	//     mov_pp oC0, r0
	// 
	// // approximately 52 instruction slots used (14 texture, 38 arithmetic)
	// 
	0xffff0200,0x05000051,0xa00f0000,0x3f000000,0xbf000000,0x00000000,0x00000000,0x0200001f,
	0x80000000,0xb0230000,0x0200001f,0x80000000,0xb0230001,0x0200001f,0x80000000,0xb0230002,
	0x0200001f,0x80000000,0xb0230003,0x0200001f,0x80000000,0xb0230004,0x0200001f,0x80000000,
	0xb0230005,0x0200001f,0x80000000,0xb0210006,0x0200001f,0x90000000,0xa00f0800,0x0200001f,
	0x90000000,0xa00f0801,0x0200001f,0x90000000,0xa00f0802,0x0200001f,0x90000000,0xa00f0803,
	0x0200001f,0x90000000,0xa00f0804,0x0200001f,0x90000000,0xa00f0805,0x03000042,0x802f0000,
	0xb0e40001,0xa0e40800,0x03000042,0x802f0001,0xb0e40001,0xa0e40802,0x03000042,0x802f0002,
	0xb0e40003,0xa0e40800,0x03000042,0x802f0003,0xb0e40003,0xa0e40802,0x03000042,0x802f0004,
	0xb0e40001,0xa0e40804,0x03000042,0x802f0005,0xb0e40003,0xa0e40804,0x03000042,0x802f0006,
	0xb0e40002,0xa0e40801,0x03000042,0x802f0007,0xb0e40002,0xa0e40803,0x03000042,0x802f0008,
	0xb0e40000,0xa0e40801,0x03000042,0x802f0009,0xb0e40000,0xa0e40803,0x03000042,0x802f000a,
	0xb0e40004,0xa0e40801,0x03000002,0x80270000,0x80e40000,0x81e40001,0x02000023,0x80270000,
	0x80e40000,0x03000002,0x80270002,0x80e40002,0x81e40003,0x02000023,0x80270002,0x80e40002,
	0x03000002,0x80270000,0x80e40000,0x80e40002,0x03000002,0x80270002,0x80e40001,0x81e40004,
	0x02000023,0x80270002,0x80e40002,0x03000002,0x80270004,0x80e40003,0x81e40005,0x02000023,
	0x80270004,0x80e40004,0x03000002,0x80270002,0x80e40002,0x80e40004,0x0300000b,0x80270004,
	0x80e40000,0x80e40002,0x03000002,0x80270000,0x80e40006,0x81e40007,0x03000002,0x80270002,
	0x80e40006,0x80e40007,0x02000023,0x80270000,0x80e40000,0x0300000b,0x80270005,0x80e40000,
	0x80e40004,0x03000002,0x80270000,0x80e40008,0x80e40009,0x04000004,0x80270000,0x80e40000,
	0xa1000000,0x80e40001,0x04000004,0x80270001,0x80e40002,0xa1000000,0x80e40001,0x03000042,
	0x802f0004,0xb0e40004,0xa0e40803,0x03000042,0x802f0006,0xb0e40005,0xa0e40805,0x03000042,
	0x802f0007,0xb0e40002,0xa0e40802,0x03000002,0x80270004,0x80e4000a,0x80e40004,0x04000004,
	0x80270004,0x80e40004,0xa1000000,0x80e40003,0x04000004,0x80270003,0x80e40002,0xa1000000,
	0x80e40003,0x0300000b,0x80270008,0x80e40000,0x80e40004,0x0300000a,0x80270009,0x80e40004,
	0x80e40000,0x0300000a,0x80270000,0x80e40001,0x80e40003,0x0300000b,0x80270004,0x80e40003,
	0x80e40001,0x0300000b,0x80270001,0x80e40004,0x80e40009,0x0300000a,0x80270003,0x80e40008,
	0x80e40000,0x0300000b,0x80270000,0x80e40005,0x80e40003,0x0300000b,0x80270003,0x80e40000,
	0x81e40001,0x04000004,0x80270000,0x80e40002,0xa0000000,0x81e40003,0x04000004,0x80270001,
	0x80e40002,0xa0000000,0x80e40003,0x0300000b,0x80270002,0x80e40000,0x80e40006,0x0300000a,
	0x80270000,0x80e40002,0x80e40001,0x02000013,0x80280000,0xb0000006,0x03000002,0x80080000,
	0x80ff0000,0xa0550000,0x04000058,0x80270000,0x80ff0000,0x80e40000,0x80e40007,0x02000001,
	0x80280000,0xa0aa0000,0x02000001,0x802f0800,0x80e40000,0x0000ffff,
};
