#include <iostream>
#include <chrono>
#include <vector>
#include <utility>
#include <algorithm>
using namespace std;

#include <stdio.h>
#include <Windows.h>

int screenWidth = 120;
int screenHeight = 40;

//float positions so that player doesn't snap from tile to tile
float playerX = 8.0f;
float playerY = 8.0f;
float movementSpeed = 1.25f;

float playerForwardAngle = 0.0f;    //in radians not degrees
float rotateSpeed = 1.0f;

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
        float oldX = playerX, oldY = playerY;

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
        //strafe
        if (GetAsyncKeyState((unsigned short)'A') & 0x8000)
        {
            //we flip sinf and cosf because cos T = sin (90 - T)
            //if you look at that on for eg: a circle (c. freya holmes igdc), the expl will be pretty evident
            //regardless this is v buggy because of sin cos and tan having different signs in different quadrants
            /*
            playerX -= cosf(playerForwardAngle) * movementSpeed * deltaTime;
            playerY -= sinf(playerForwardAngle) * movementSpeed * deltaTime;
            */
            
            //so we try new technique by finding perpendicular unit vector
            //basically doing this in a different way and not using just a 'nifty' hack ^
            float tempX = sinf(playerForwardAngle);
            float tempY = cosf(playerForwardAngle);

            playerX -= tempY * movementSpeed * deltaTime;
            playerY -= -tempX * movementSpeed * deltaTime;
        }
        if (GetAsyncKeyState((unsigned short)'D') & 0x8000)
        {
            /*
            playerX += cosf(playerForwardAngle) * movementSpeed * deltaTime;
            playerY += sinf(playerForwardAngle) * movementSpeed * deltaTime;
            */
            float tempX = sinf(playerForwardAngle);
            float tempY = cosf(playerForwardAngle);

            playerX += tempY * movementSpeed * deltaTime;
            playerY += -tempX * movementSpeed * deltaTime;
        }

        //collision detection
        if (map[(int)playerY * mapWidth + (int)playerX] == '#')
        {
            playerX = oldX;
            playerY = oldY;
        }

        //rotation
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            playerForwardAngle -= rotateSpeed * deltaTime;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            playerForwardAngle += rotateSpeed * deltaTime;

        //screenwidth because for every frame, we work on, we calculate a ray for every pixel (/ column) on screen
        for (int x = 0; x < screenWidth; x++)
        {
            //calculate current ray angle in world space
            float rayAngle = (playerForwardAngle - FOV / 2) //calculate the lowest angle value in FOV
                + ((float)x / (float)screenWidth) * FOV;    //add iterated ray i, to calculate ray angle

            float distanceToWall = 0.0f;
            bool hasHitWall = false;
            bool isEdge = false;

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
                    //if the block is a wall
                    if (map[testY * mapWidth + testX] == '#')//access the string as a 1D array, splitting it by 
                    {
                        hasHitWall = true;


                        //edge detection
                        #pragma region edge detection explanation
                        //we calculate paths from all (4) corner vertices of current block (#)
                        //to find if angles (from player, between corner and current raycast(makes a triangle hence cos)) are acute enough
                        //if they are acute enough, they are on edges (draw on paint and see)
                        //this is because rays towards the center of a block (eg: 0.5f away) would create a less acute triangle than
                        //for eg 0.1f
                        #pragma endregion
                        vector<pair<float, float>> pairs;   //distance and dot product

                        //look at all 4 corners
                        for(int tx = 0; tx < 2; tx++)
                            for (int ty = 0; ty < 2; ty++)
                            {
                                //unit vector of current corner vertex
                                float vx = (float)testX + tx - playerX;
                                float vy = (float)testY + ty - playerY;
                                float distance = sqrt(vx * vx + vy * vy);
                                float dot = (eyeX * vx / distance) + (eyeY * vy / distance);
                                pairs.push_back(make_pair(distance, dot));
                            }

                        //sort pairs from closest to farthest distances (first element in pair)
                        sort(pairs.begin(), pairs.end(), [](const pair<float, float>& left, const pair<float, float>& right) {return left.first < right.first; });

                        //since our vector pairs is sorted, the first two elements are the closes
                        //and hence they will have boundaries visible
                        //max angle between ray, corner and player, theta at player
                        float maxAngleBetweenRayAndCorner = 0.005f;
                        //acos is inverse cosine (cos^-1), cos inv of a dot product of 2 vectors gives you the angle between them
                        if (acos(pairs.at(0).second) < maxAngleBetweenRayAndCorner) isEdge = true;
                        if (acos(pairs.at(1).second) < maxAngleBetweenRayAndCorner) isEdge = true;
                        //if (acos(pairs.at(2).second) < maximumAngleForBoundary) isEdge = true;
                    }
                }
            }

            //walls further away from the cam are shorter and closer are larger
            int ceilingPos = (float)(screenHeight / 2) - screenHeight / ((float)distanceToWall);
            int floorPos = screenHeight - ceilingPos;

            short myShade = ' ';                          //assume its too far away
            
            //shading walls using ext unicode symbols
            if (distanceToWall <= maxDepth / 4.0f)
                myShade = 0x2588;
            else if (distanceToWall < maxDepth / 3.0f)
                myShade = 0x2593;
            else if (distanceToWall < maxDepth / 2.0f)
                myShade = 0x2592;
            else if (distanceToWall < maxDepth)
                myShade = 0x2591;

            if(isEdge)
                myShade = ' ';
                

            for (int y = 0; y < screenHeight; y++)
            {
                if (y <= ceilingPos)
                    screen[y * screenWidth + x] = ' ';      //ceiling should be shaded blank
                else if (y > ceilingPos && y <= floorPos)   //wall
                    screen[y * screenWidth + x] = myShade;
                else
                {
                    //shading floor to give depth to scene
                    //calculating how far away is the current cell (y) from vertical center of screen
                    //this will tell how far away it is from camera, bc we know it is a floor cell already
                    float floorCellDistance = 1.0f - (((float)y - screenHeight / 2.0f) / ((float)screenHeight / 2.0f));

                    if (floorCellDistance < 0.25f)      myShade = '#';
                    else if (floorCellDistance < 0.5f)  myShade = 'x';
                    else if (floorCellDistance < 0.75f) myShade = '.';
                    else if (floorCellDistance < 0.9f)  myShade = '-';
                    else                                myShade = ' ';

                    screen[y * screenWidth + x] = myShade;
                }
            }
        }

        //Display stats
        //         buffer, bufflen, string,                               args
        swprintf_s(screen, 40, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f", playerX, playerY, playerForwardAngle * 180 / 3.14f, 1.0f / deltaTime);

        //Display Maps
        for(int mx = 0; mx < mapWidth; mx++)
            for (int my = 0; my < mapWidth; my++)
            {
                screen[(my + 1) * screenWidth + mx] = map[my * mapWidth + (mapWidth - mx - 1)];
                //screen[(my + 1) * screenWidth + mx] = map[my * mapWidth + mx];
            }

        //display player in map
        screen[((int)playerY + 1) * screenWidth + (int)(mapWidth - playerX)] = 'P';
        //screen[((int)playerY + 1) * screenWidth + (int)playerX] = 'P';

        //setting the last wchar of this array to terminate so it knows when to end
        screen[screenWidth * screenHeight - 1] = '\0';
        //writing character in the screen inside hConsole of x*y at co-ord so that it doesn't scroll down
        WriteConsoleOutputCharacter(hConsole, screen, screenHeight * screenWidth, { 0, 0 }, &dwBytesWritten);
    }

    return 0;
}