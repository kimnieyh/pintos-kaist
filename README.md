Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.

The manual is available at https://casys-kaist.github.io/pintos-kaist/.


### mmap-exit 
mmap을 한 경우,
munmap할 때 writable 한 경우에는 지금까지 kva의 데이터 변경사항을
file에 적용해주어야 한다. 
exit할 때도 file의 변경사항을 적용해주어어야 한다.
file_destroy 함수에 
file swap_out 을 진행하도록 하면서 적용 되도록 수정해주어서 해결하였다. 

