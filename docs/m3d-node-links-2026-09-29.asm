; ArmadaL.exe SHA256 63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36
; Image base 0x400000. Child +0x1c, next sibling +0x20, parent +0x18.


ArmadaL.exe:     file format pei-i386


Disassembly of section .text:

006386c8 <.text+0x2376c8>:
  6386c8:	8b 45 0c             	mov    eax,DWORD PTR [ebp+0xc]
  6386cb:	85 c0                	test   eax,eax
  6386cd:	74 10                	je     0x6386df
  6386cf:	85 db                	test   ebx,ebx
  6386d1:	74 0c                	je     0x6386df
  6386d3:	89 43 18             	mov    DWORD PTR [ebx+0x18],eax
  6386d6:	8b 48 1c             	mov    ecx,DWORD PTR [eax+0x1c]
  6386d9:	89 4b 20             	mov    DWORD PTR [ebx+0x20],ecx
  6386dc:	89 58 1c             	mov    DWORD PTR [eax+0x1c],ebx
  6386df:	8b 4d f4             	mov    ecx,DWORD PTR [ebp-0xc]
  6386e2:	5f                   	pop    edi
  6386e3:	8b c3                	mov    eax,ebx
  6386e5:	5e                   	pop    esi
  6386e6:	5b                   	pop    ebx
  6386e7:	64 89 0d 00 00 00 00 	mov    DWORD PTR fs:0x0,ecx
  6386ee:	8b e5                	mov    esp,ebp
  6386f0:	5d                   	pop    ebp
  6386f1:	c2 10 00             	ret    0x10


ArmadaL.exe:     file format pei-i386


Disassembly of section .text:

00639db9 <.text+0x238db9>:
  639db9:	8b 5b 1c             	mov    ebx,DWORD PTR [ebx+0x1c]
  639dbc:	85 db                	test   ebx,ebx
  639dbe:	74 23                	je     0x639de3
  639dc0:	8b 45 0c             	mov    eax,DWORD PTR [ebp+0xc]
  639dc3:	83 ec 30             	sub    esp,0x30
  639dc6:	b9 0c 00 00 00       	mov    ecx,0xc
  639dcb:	8d 75 10             	lea    esi,[ebp+0x10]
  639dce:	8b fc                	mov    edi,esp
  639dd0:	50                   	push   eax
  639dd1:	f3 a5                	rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]
  639dd3:	53                   	push   ebx
  639dd4:	e8 f7 fe ff ff       	call   0x639cd0
  639dd9:	8b 5b 20             	mov    ebx,DWORD PTR [ebx+0x20]
  639ddc:	83 c4 38             	add    esp,0x38
  639ddf:	85 db                	test   ebx,ebx
  639de1:	75 dd                	jne    0x639dc0
