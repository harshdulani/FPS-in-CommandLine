#include <iostream>
#include<chrono>
using namespace std;

#include <Windows.h>

int screenWidth = 120;
int screenHeight = 40;

//float positions so that player doesn't snap from tile to tile
float playerX = 8.0f;
float playerY = 8.0f;
float movementSpeed = 0.5f;

float playerForwardAngle = 0.0f;
float rotateSpeed = 0.75f;

int mapHeight = 16;
int mapWidth = 16;

//45 degrees
float FOV = 3.14156 / 4;
float maxDepth = (float) max(mapWidth, mapHeight);

int main()
{
    //creating a variable that stores w*h wide char variables
    //wide chars are used to store value upto 65536 
    //used to display unicode chars on command prompt, as opposed to ASCII 128
    wchar_t* screen = new wchar_t[screenWidth * screenHeight];

    //creating a handle to a buffer of the console screen
    //call the function to see its just asking for rw permissions and 
    HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    SetConsoleActiveScreenBuffer(hConsole);
    DWORD dwBytesWritten = 0;

    //wide string because unicode
    wstring map;

    map += L"################";
    map += L"#..............#";
    map += L"#..............#";
    map += L"#.........#....#";
    map += L"#.........#....#";
    map += L"#..............#";
    map += L"#..............#";
    map += L"#..............#";
    map += L"#..............#";
    map += L"#..............#";
    map += L"#..............#";
    map += L"#..............#";
    map += L"#........#######";
    map += L"#..............#";
    map += L"#..............#";
    map += L"################";

    auto timePoint1 = chrono::system_clock::now();
    auto timePoint2 = chrono::system_clock::now();

    //Game logic
    while (1)
    {
        //calculating time delta between frames
        timePoint2 = chrono::system_clock::now();
        chrono::duration<float> delta = timePoint2 - timePoint1;
        timePoint1 = timePoint2;
        float deltaTime = delta.count();

        //movement
        #pragma region explanation for GetAsyncKeyState
        //async returns a short with the high bit set to 1 if the key is down
        //every bit except that may give other (usually irrelevant, here def irrelevant) info,
        //ANDing it w 0x8000 means ANDing w same size binary number except that all bits are set
        //so now we a clean slate to check if the A button is pressed
        //since we only care about pressed status, having other bits possibly turned on might interfere with
        //if(zero or non zero)
#pragma endregion
        if (GetAsyncKeyState((unsigned short)'W') & 0x8000)
        {
            playerX += sinf(playerForwardAngle) * movementSpeed * deltaTime;
            playerY += cosf(playerForwardAngle) * movementSpeed * deltaTime;
        }
        if (GetAsyncKeyState((unsigned short)'S') & 0x8000)
        {
            playerX -= sinf(playerForwardAngle) * movementSpeed * deltaTime;
            playerY -= cosf(playerForwardAngle) * movementSpeed * deltaTime;
        }

        //rotation
        if (GetAsyncKeyState((unsigned short)'A') & 0x8000)
            playerForwardAngle -= rotateSpeed * deltaTime;
        if (GetAsyncKeyState((unsigned short)'D') & 0x8000)
            playerForwardAngle += rotateSpeed * deltaTime;

        //only concerning ourself w width because no vertical cam movement
        //screenwidth because for every frame, we work on, we calculate a ray for every pixel (/ column) on screen
        for (int x = 0; x < screenWidth; x++)
        {
            //calculate current ray angle in world space
            float rayAngle = (playerForwardAngle - FOV / 2) //calculate the lowest angle value in FOV
                + ((float)x / (float)screenWidth) * FOV;    //add iterated ray i, to calculate ray angle

            float distanceToWall = 0.0f;
            bool hasHitWall = false;

            //unit vector for ray in player space, dir that player is looking in
            float eyeX = sinf(rayAngle);
            float eyeY = cosf(rayAngle);

            while (!hasHitWall && distanceToWall < maxDepth)
            {
                distanceToWall += 0.1f;

                //draw a line from playerPos to unitVector * current magnitude
                int testX = (int)(playerX + eyeX * distanceToWall);
                int testY = (int)(playerY + eyeY * distanceToWall);

                //test if ray is out of bounds
                if (testX < 0 || testX >= mapWidth || testY < 0 || testY >= mapHeight)
                {
                    hasHitWall = true;
                    distanceToWall = maxDepth;
                }
                else
                {
                    //access the string as a 1D array, splitting it by 
                    if (map[testY * mapWidth + testX] == '#')
                    {
                        hasHitWall = true;
                    }
                }
            }

            //walls further away from the cam are shorter and closer are larger
            int ceilingPos = (float)(screenHeight / 2) - screenHeight / ((float)distanceToWall);
            int floorPos = screenHeight - ceilingPos;

            short wallShade = ' ';                          //assume its too far away
            short floorShade = ' ';

            //shading walls using ext unicode symbols
            if (distanceToWall <= maxDepth / 4.0f)
                wallShade = 0x2588;
            else if (distanceToWall < maxDepth / 3.0f)
                wallShade = 0x2593;
            else if (distanceToWall < maxDepth / 2.0f)
                wallShade = 0x2592;
            else if (distanceToWall < maxDepth)
                wallShade = 0x2591;

            for (int y = 0; y < screenHeight; y++)
            {
                if (y < ceilingPos)
                    screen[y * screenWidth + x] = ' ';      //ceiling should be shaded blank
                else if (y > ceilingPos && y <= floorPos)   //wall
                    screen[y * screenWidth + x] = wallShade;
                else
                {
                    //shading floor to give depth to scene
                    //calculating how far away is the current cell (y) from vertical center of screen
                    //this will tell how far away it is from camera, bc we know it is a floor cell already
                    float floorCellDistance = 1.0f - (((float)y - screenHeight / 2.0f) / ((float)screenHeight / 2.0f));

                    if (floorCellDistance < 0.25f)      floorShade = '#';
                    else if (floorCellDistance < 0.5f)  floorShade = 'x';
                    else if (floorCellDistance < 0.75f) floorShade = '.';
                    else if (floorCellDistance < 0.9f)  floorShade = '-';

                    screen[y * screenWidth + x] = floorShade;
                }
            }
        }


        //setting the last wchar of this array to terminate so it knows when to end
        screen[screenWidth * screenHeight - 1] = '\0';
        //writing character in the screen inside hConsole of x*y at co-ord so that it doesn't scroll down
        WriteConsoleOutputCharacter(hConsole, screen, screenHeight * screenWidth, { 0, 0 }, &dwBytesWritten);
    }

    return 0;
}