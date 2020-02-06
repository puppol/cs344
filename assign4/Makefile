build:
	gcc -o keygen keygen.c -g
	gcc -o otp_enc_d otp_enc_d.c -pthread -g
	gcc -o otp_enc otp_enc.c -g
	gcc -o otp_dec otp_dec.c -g
	gcc -o otp_dec_d otp_dec_d.c -pthread -g

start:
	otp_enc_d 2345 &
	otp_dec_d 3456 &

clean:
	rm -f keygen
	rm -f otp_dec
	rm -f otp_dec_d
	rm -f otp_enc
	rm -f otp_enc_d
	rm -f ciphertext*
	rm -f plaintext*_*
