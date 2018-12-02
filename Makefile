all: ext2_mkdir.c ext2_cp.c ext2_ln.c ext2_rm.c readimage.c helpers.c
		gcc ext2_mkdir.c helpers.c -o ext2_mkdir
		gcc ext2_cp.c helpers.c -o ext2_cp
		gcc ext2_ln.c helpers.c -o ext2_ln
		gcc ext2_rm.c helpers.c -o ext2_rm
		gcc readimage.c -o readimage