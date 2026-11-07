
ArmadaL.exe:     file format pei-i386


Disassembly of section .text:

004bf7e6 <.text+0xbe7e6>:
  4bf7e6:	8b be d8 01 00 00    	mov    edi,DWORD PTR [esi+0x1d8]
  4bf7ec:	6a 04                	push   0x4
  4bf7ee:	68 94 32 70 00       	push   0x703294
  4bf7f3:	8b cf                	mov    ecx,edi
  4bf7f5:	e8 56 0f 16 00       	call   0x620750
  4bf7fa:	6a 04                	push   0x4
  4bf7fc:	68 9c 32 70 00       	push   0x70329c
  4bf801:	8b cf                	mov    ecx,edi
  4bf803:	89 86 08 04 00 00    	mov    DWORD PTR [esi+0x408],eax
  4bf809:	e8 42 0f 16 00       	call   0x620750
  4bf80e:	6a 04                	push   0x4
  4bf810:	68 a4 32 70 00       	push   0x7032a4
  4bf815:	8b cf                	mov    ecx,edi
  4bf817:	89 86 18 04 00 00    	mov    DWORD PTR [esi+0x418],eax
  4bf81d:	e8 2e 0f 16 00       	call   0x620750
  4bf822:	6a 04                	push   0x4
  4bf824:	68 ac 32 70 00       	push   0x7032ac
  4bf829:	8b cf                	mov    ecx,edi
  4bf82b:	89 86 1c 04 00 00    	mov    DWORD PTR [esi+0x41c],eax
  4bf831:	e8 1a 0f 16 00       	call   0x620750
  4bf836:	6a 04                	push   0x4
  4bf838:	68 b4 32 70 00       	push   0x7032b4
  4bf83d:	8b cf                	mov    ecx,edi
  4bf83f:	89 86 14 04 00 00    	mov    DWORD PTR [esi+0x414],eax
  4bf845:	e8 06 0f 16 00       	call   0x620750
  4bf84a:	6a 04                	push   0x4
  4bf84c:	68 bc 32 70 00       	push   0x7032bc
  4bf851:	8b cf                	mov    ecx,edi
  4bf853:	89 86 0c 04 00 00    	mov    DWORD PTR [esi+0x40c],eax
  4bf859:	e8 f2 0e 16 00       	call   0x620750
  4bf85e:	6a 04                	push   0x4
  4bf860:	68 c4 32 70 00       	push   0x7032c4
  4bf865:	8b cf                	mov    ecx,edi
  4bf867:	89 86 10 04 00 00    	mov    DWORD PTR [esi+0x410],eax
  4bf86d:	e8 de 0e 16 00       	call   0x620750
  4bf872:	89 86 20 04 00 00    	mov    DWORD PTR [esi+0x420],eax

ArmadaL.exe:     file format pei-i386

Contents of section .data:
 703290 696f6e00 63726577 00000000 656e6769  ion.crew....engi
 7032a0 6e657300 6c696665 00000000 73656e73  nes.life....sens
 7032b0 6f727300 73686965 6c647300 74617267  ors.shields.targ
 7032c0 65740000 626f7267 00000000           et..borg....    

ArmadaL.exe:     file format pei-i386


Disassembly of section .text:

004cb090 <.text+0xca090>:
  4cb090:	56                   	push   esi
  4cb091:	8b f1                	mov    esi,ecx
  4cb093:	e8 d8 a8 00 00       	call   0x4d5970
  4cb098:	b0 01                	mov    al,0x1
  4cb09a:	33 c9                	xor    ecx,ecx
  4cb09c:	88 86 9c 00 00 00    	mov    BYTE PTR [esi+0x9c],al
  4cb0a2:	88 86 9d 00 00 00    	mov    BYTE PTR [esi+0x9d],al
  4cb0a8:	88 86 9e 00 00 00    	mov    BYTE PTR [esi+0x9e],al
  4cb0ae:	88 86 9f 00 00 00    	mov    BYTE PTR [esi+0x9f],al
  4cb0b4:	88 86 a0 00 00 00    	mov    BYTE PTR [esi+0xa0],al
  4cb0ba:	c7 06 2c 3e 6b 00    	mov    DWORD PTR [esi],0x6b3e2c
  4cb0c0:	89 8e 98 00 00 00    	mov    DWORD PTR [esi+0x98],ecx
  4cb0c6:	88 8e a1 00 00 00    	mov    BYTE PTR [esi+0xa1],cl
  4cb0cc:	c7 86 a4 00 00 00 00 	mov    DWORD PTR [esi+0xa4],0x3f800000
  4cb0d3:	00 80 3f 
  4cb0d6:	8b c6                	mov    eax,esi
  4cb0d8:	5e                   	pop    esi
  4cb0d9:	c3                   	ret
  4cb0da:	90                   	nop
  4cb0db:	90                   	nop
  4cb0dc:	90                   	nop
  4cb0dd:	90                   	nop
  4cb0de:	90                   	nop
  4cb0df:	90                   	nop
  4cb0e0:	55                   	push   ebp
  4cb0e1:	8b ec                	mov    ebp,esp
  4cb0e3:	51                   	push   ecx
  4cb0e4:	53                   	push   ebx
  4cb0e5:	56                   	push   esi
  4cb0e6:	8b f1                	mov    esi,ecx
  4cb0e8:	8b 9e 98 00 00 00    	mov    ebx,DWORD PTR [esi+0x98]
  4cb0ee:	8b 86 80 00 00 00    	mov    eax,DWORD PTR [esi+0x80]
  4cb0f4:	8b 8b dc 01 00 00    	mov    ecx,DWORD PTR [ebx+0x1dc]
  4cb0fa:	3b c1                	cmp    eax,ecx
  4cb0fc:	8b 48 30             	mov    ecx,DWORD PTR [eax+0x30]
  4cb0ff:	74 28                	je     0x4cb129
  4cb101:	85 c9                	test   ecx,ecx
  4cb103:	0f 84 09 02 00 00    	je     0x4cb312
  4cb109:	8b 91 bc 00 00 00    	mov    edx,DWORD PTR [ecx+0xbc]
  4cb10f:	83 ca 01             	or     edx,0x1
  4cb112:	89 91 bc 00 00 00    	mov    DWORD PTR [ecx+0xbc],edx
  4cb118:	8b 40 30             	mov    eax,DWORD PTR [eax+0x30]
  4cb11b:	8b 88 bc 00 00 00    	mov    ecx,DWORD PTR [eax+0xbc]
  4cb121:	83 c9 02             	or     ecx,0x2
  4cb124:	e9 e3 01 00 00       	jmp    0x4cb30c
  4cb129:	85 c9                	test   ecx,ecx
  4cb12b:	74 19                	je     0x4cb146
  4cb12d:	8b 91 bc 00 00 00    	mov    edx,DWORD PTR [ecx+0xbc]
  4cb133:	83 e2 fe             	and    edx,0xfffffffe
  4cb136:	89 91 bc 00 00 00    	mov    DWORD PTR [ecx+0xbc],edx
  4cb13c:	8b 40 30             	mov    eax,DWORD PTR [eax+0x30]
  4cb13f:	83 a0 bc 00 00 00 fd 	and    DWORD PTR [eax+0xbc],0xfffffffd
  4cb146:	57                   	push   edi
  4cb147:	8b bb 08 04 00 00    	mov    edi,DWORD PTR [ebx+0x408]
  4cb14d:	85 ff                	test   edi,edi
  4cb14f:	74 03                	je     0x4cb154
  4cb151:	8b 7f 1c             	mov    edi,DWORD PTR [edi+0x1c]
  4cb154:	d9 86 a4 00 00 00    	fld    DWORD PTR [esi+0xa4]
  4cb15a:	85 ff                	test   edi,edi
  4cb15c:	74 56                	je     0x4cb1b4
  4cb15e:	dc 15 f0 e8 6a 00    	fcom   QWORD PTR ds:0x6ae8f0
  4cb164:	df e0                	fnstsw ax
  4cb166:	f6 c4 01             	test   ah,0x1
  4cb169:	75 07                	jne    0x4cb172
  4cb16b:	b8 01 00 00 00       	mov    eax,0x1
  4cb170:	eb 02                	jmp    0x4cb174
  4cb172:	33 c0                	xor    eax,eax
  4cb174:	8b 8f bc 00 00 00    	mov    ecx,DWORD PTR [edi+0xbc]
  4cb17a:	25 ff 00 00 00       	and    eax,0xff
  4cb17f:	d8 05 10 ea 6a 00    	fadd   DWORD PTR ds:0x6aea10
  4cb185:	83 e1 fe             	and    ecx,0xfffffffe
  4cb188:	0b c8                	or     ecx,eax
  4cb18a:	51                   	push   ecx
  4cb18b:	89 8f bc 00 00 00    	mov    DWORD PTR [edi+0xbc],ecx
  4cb191:	8b 57 20             	mov    edx,DWORD PTR [edi+0x20]
  4cb194:	d9 1c 24             	fstp   DWORD PTR [esp]
  4cb197:	52                   	push   edx
  4cb198:	e8 93 01 00 00       	call   0x4cb330
  4cb19d:	d9 5d fc             	fstp   DWORD PTR [ebp-0x4]
  4cb1a0:	8b 45 fc             	mov    eax,DWORD PTR [ebp-0x4]
  4cb1a3:	8b 4f 1c             	mov    ecx,DWORD PTR [edi+0x1c]
  4cb1a6:	50                   	push   eax
  4cb1a7:	51                   	push   ecx
  4cb1a8:	e8 83 01 00 00       	call   0x4cb330
  4cb1ad:	dd d8                	fstp   st(0)
  4cb1af:	83 c4 10             	add    esp,0x10
  4cb1b2:	eb 02                	jmp    0x4cb1b6
  4cb1b4:	dd d8                	fstp   st(0)
  4cb1b6:	8b 83 0c 04 00 00    	mov    eax,DWORD PTR [ebx+0x40c]
  4cb1bc:	5f                   	pop    edi
  4cb1bd:	85 c0                	test   eax,eax
  4cb1bf:	74 25                	je     0x4cb1e6
  4cb1c1:	8a 96 9c 00 00 00    	mov    dl,BYTE PTR [esi+0x9c]
  4cb1c7:	88 55 fc             	mov    BYTE PTR [ebp-0x4],dl
  4cb1ca:	8b 90 bc 00 00 00    	mov    edx,DWORD PTR [eax+0xbc]
  4cb1d0:	8b 4d fc             	mov    ecx,DWORD PTR [ebp-0x4]
  4cb1d3:	83 e2 fd             	and    edx,0xfffffffd
  4cb1d6:	81 e1 ff 00 00 00    	and    ecx,0xff
  4cb1dc:	d1 e1                	shl    ecx,1
  4cb1de:	0b ca                	or     ecx,edx
  4cb1e0:	89 88 bc 00 00 00    	mov    DWORD PTR [eax+0xbc],ecx
  4cb1e6:	8b 83 18 04 00 00    	mov    eax,DWORD PTR [ebx+0x418]
  4cb1ec:	85 c0                	test   eax,eax
  4cb1ee:	74 25                	je     0x4cb215
  4cb1f0:	8a 8e 9d 00 00 00    	mov    cl,BYTE PTR [esi+0x9d]
  4cb1f6:	88 4d fc             	mov    BYTE PTR [ebp-0x4],cl
  4cb1f9:	8b 88 bc 00 00 00    	mov    ecx,DWORD PTR [eax+0xbc]
  4cb1ff:	8b 55 fc             	mov    edx,DWORD PTR [ebp-0x4]
  4cb202:	83 e1 fd             	and    ecx,0xfffffffd
  4cb205:	81 e2 ff 00 00 00    	and    edx,0xff
  4cb20b:	d1 e2                	shl    edx,1
  4cb20d:	0b d1                	or     edx,ecx
  4cb20f:	89 90 bc 00 00 00    	mov    DWORD PTR [eax+0xbc],edx
  4cb215:	8b 83 10 04 00 00    	mov    eax,DWORD PTR [ebx+0x410]
  4cb21b:	85 c0                	test   eax,eax
  4cb21d:	74 25                	je     0x4cb244
  4cb21f:	8a 96 9e 00 00 00    	mov    dl,BYTE PTR [esi+0x9e]
  4cb225:	88 55 fc             	mov    BYTE PTR [ebp-0x4],dl
  4cb228:	8b 90 bc 00 00 00    	mov    edx,DWORD PTR [eax+0xbc]
  4cb22e:	8b 4d fc             	mov    ecx,DWORD PTR [ebp-0x4]
  4cb231:	83 e2 fd             	and    edx,0xfffffffd
  4cb234:	81 e1 ff 00 00 00    	and    ecx,0xff
  4cb23a:	d1 e1                	shl    ecx,1
  4cb23c:	0b ca                	or     ecx,edx
  4cb23e:	89 88 bc 00 00 00    	mov    DWORD PTR [eax+0xbc],ecx
  4cb244:	8b 83 1c 04 00 00    	mov    eax,DWORD PTR [ebx+0x41c]
  4cb24a:	85 c0                	test   eax,eax
  4cb24c:	74 25                	je     0x4cb273
  4cb24e:	8a 8e a0 00 00 00    	mov    cl,BYTE PTR [esi+0xa0]
  4cb254:	88 4d fc             	mov    BYTE PTR [ebp-0x4],cl
  4cb257:	8b 88 bc 00 00 00    	mov    ecx,DWORD PTR [eax+0xbc]
  4cb25d:	8b 55 fc             	mov    edx,DWORD PTR [ebp-0x4]
  4cb260:	83 e1 fd             	and    ecx,0xfffffffd
  4cb263:	81 e2 ff 00 00 00    	and    edx,0xff
  4cb269:	d1 e2                	shl    edx,1
  4cb26b:	0b d1                	or     edx,ecx
  4cb26d:	89 90 bc 00 00 00    	mov    DWORD PTR [eax+0xbc],edx
  4cb273:	8b 83 14 04 00 00    	mov    eax,DWORD PTR [ebx+0x414]
  4cb279:	85 c0                	test   eax,eax
  4cb27b:	74 25                	je     0x4cb2a2
  4cb27d:	8a 96 9f 00 00 00    	mov    dl,BYTE PTR [esi+0x9f]
  4cb283:	88 55 fc             	mov    BYTE PTR [ebp-0x4],dl
  4cb286:	8b 90 bc 00 00 00    	mov    edx,DWORD PTR [eax+0xbc]
  4cb28c:	8b 4d fc             	mov    ecx,DWORD PTR [ebp-0x4]
  4cb28f:	83 e2 fd             	and    edx,0xfffffffd
  4cb292:	81 e1 ff 00 00 00    	and    ecx,0xff
  4cb298:	d1 e1                	shl    ecx,1
  4cb29a:	0b ca                	or     ecx,edx
  4cb29c:	89 88 bc 00 00 00    	mov    DWORD PTR [eax+0xbc],ecx
  4cb2a2:	8a 86 a1 00 00 00    	mov    al,BYTE PTR [esi+0xa1]
  4cb2a8:	8b cb                	mov    ecx,ebx
  4cb2aa:	50                   	push   eax
  4cb2ab:	e8 c0 48 ff ff       	call   0x4bfb70
  4cb2b0:	8b 83 20 04 00 00    	mov    eax,DWORD PTR [ebx+0x420]
  4cb2b6:	85 c0                	test   eax,eax
  4cb2b8:	74 58                	je     0x4cb312
  4cb2ba:	8a 96 a1 00 00 00    	mov    dl,BYTE PTR [esi+0xa1]
  4cb2c0:	84 d2                	test   dl,dl
  4cb2c2:	8b 90 bc 00 00 00    	mov    edx,DWORD PTR [eax+0xbc]
  4cb2c8:	0f 94 c1             	sete   cl
  4cb2cb:	88 4d fc             	mov    BYTE PTR [ebp-0x4],cl
  4cb2ce:	83 e2 fe             	and    edx,0xfffffffe
  4cb2d1:	8b 4d fc             	mov    ecx,DWORD PTR [ebp-0x4]
  4cb2d4:	81 e1 ff 00 00 00    	and    ecx,0xff
  4cb2da:	0b d1                	or     edx,ecx
  4cb2dc:	89 90 bc 00 00 00    	mov    DWORD PTR [eax+0xbc],edx
  4cb2e2:	8a 86 a1 00 00 00    	mov    al,BYTE PTR [esi+0xa1]
  4cb2e8:	84 c0                	test   al,al
  4cb2ea:	8b 83 20 04 00 00    	mov    eax,DWORD PTR [ebx+0x420]
  4cb2f0:	0f 94 c2             	sete   dl
  4cb2f3:	88 55 fc             	mov    BYTE PTR [ebp-0x4],dl
  4cb2f6:	8b 90 bc 00 00 00    	mov    edx,DWORD PTR [eax+0xbc]
  4cb2fc:	8b 4d fc             	mov    ecx,DWORD PTR [ebp-0x4]
  4cb2ff:	83 e2 fd             	and    edx,0xfffffffd
  4cb302:	81 e1 ff 00 00 00    	and    ecx,0xff
  4cb308:	d1 e1                	shl    ecx,1
  4cb30a:	0b ca                	or     ecx,edx
  4cb30c:	89 88 bc 00 00 00    	mov    DWORD PTR [eax+0xbc],ecx
  4cb312:	8b 45 08             	mov    eax,DWORD PTR [ebp+0x8]
  4cb315:	8b ce                	mov    ecx,esi
  4cb317:	50                   	push   eax
  4cb318:	e8 93 a6 00 00       	call   0x4d59b0
  4cb31d:	5e                   	pop    esi
  4cb31e:	5b                   	pop    ebx
  4cb31f:	8b e5                	mov    esp,ebp
  4cb321:	5d                   	pop    ebp
  4cb322:	c2                   	.byte 0xc2
  4cb323:	04                   	.byte 0x4
