import random
import string
line_length=127;
lines_count=100;
line2 = 'abcdefghijklmnopqrstuwxyzabcdefghijklmnopqrstuwxyzabcdefghijklmnopqrstuwxyzabcdefghijklmnopqrstuwxyz'
# for i in range(0,lines_count):
#     line = str(i) + ' ' + ''.join(random.choice(string.ascii_lowercase) for _ in range(line_length))
#     print(line)

for i in range(0,lines_count):
    line = str(i) + ' ' + line2;
    print(line)