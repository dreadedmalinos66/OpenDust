# OpenDust
OpenDust is rom / os for esp32 built mainly for research

# Supported boards
generic esp-wroom-32

esp32-2432S028 (cheap yellow display)


# Updates

9/12/26

going public, there is no reason to keep project private


9/13/26

removing automatic tft_init(); function from code, it can be re enabled by adding tft_init(); in the void setup() function

9/14/26

deciding to not add test feature "wget" into new builds, for some reason it totaled my sd card by making weird changes, filesystem was changed to read only and full /private/ dirrectory was replaced with weird 1.7gb extentionless file

expect version 0.3 to arrive today or tommorow (9/15/26)

9/15/26

as promised, release 0.3 is here, added some experimental features and some quality of life features, or whatever you call them my english is bad

9/22/26

improved one function, college is hard for me so dont expect a lot of releases or bug fixes, next update will bring optimisation
