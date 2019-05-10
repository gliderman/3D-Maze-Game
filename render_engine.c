#include "render_engine.h"
#include <math.h>
#include <stdlib.h>
#include "subsystem.h"
#include "uart.h"
#include "terminal.h"

#define M_PI 3.14159265358979323846

// Rendering helper functions
point_t pointToScreen(vector_t delta,
        rounding_t camHAngle, rounding_t camVAngle,
        rounding_t angleHPixel, rounding_t angleVPixel,
        uint8_t halfWidth, uint8_t halfHeight);
rounding_t dotProduct(vector_t a, vector_t b);
static camera_t compareCamera;
int compareTriangles(const void *a, const void *b);
void paintPixel(framebuffer_t *frame, uint16_t x, uint16_t y, uint8_t color);
void paintPixelf(framebuffer_t *frame, rounding_t x, rounding_t y, uint8_t color);

// UART helper functions
void changeTerminalCursorLocation(uint8_t channel, uint8_t x, uint8_t y);
void writeTerminalNumber(uint8_t channel, uint8_t number);
void changeTerminalColor(uint8_t channel, uint8_t color);
void writeTerminalBlock(uint8_t channel, uint8_t data);

void Render_Engine_RenderFrame(world_t *world, camera_t *camera, framebuffer_t *frame) {
    uint16_t bufLength = frame->width * frame->height;
    uint8_t halfWidth = frame->width / 2;
    uint8_t halfHeight = frame->height / 2;
    rounding_t anglePerPixelHorizontal = (camera->fovHorizontal * M_PI) /
            (frame->width * 180.0);
    rounding_t anglePerPixelVertical = (camera->fovVertical * M_PI) /
            (frame->height * 180.0);
    rounding_t cameraHorizontalAngle = camera->rotation.z;
    if (camera->rotation.z < 0) {
        cameraHorizontalAngle = -cameraHorizontalAngle;
    }
    cameraHorizontalAngle = fmod(cameraHorizontalAngle + 180, 360) - 180;
    if (camera->rotation.z < 0) {
        cameraHorizontalAngle = -cameraHorizontalAngle;
    }
    cameraHorizontalAngle = cameraHorizontalAngle * (M_PI / 180.0);
    rounding_t cameraVerticalAngle = (camera->rotation.y * M_PI) / 180.0;
    vector_t cameraDirection = {cos(cameraHorizontalAngle),
            sin(cameraHorizontalAngle),
            ((cameraVerticalAngle <= -90) || (cameraVerticalAngle >= 90)) ? tan(cameraVerticalAngle) : ((cameraVerticalAngle > 0) - (cameraVerticalAngle < 0)) * 10000};
    uint16_t i;
    
    // Set the framebuffer to the background color
    for (i = 0; i < bufLength; i++) {
        frame->buffer[i] = world->backgroundColor;
    }
    
    // Sort triangles by distance to the camera
    triangle_t triangles[world->numTriangles];
    for (i = 0; i < world->numTriangles; i++) {
        triangles[i].color = world->triangles[i].color;
        triangles[i].p1 = world->triangles[i].p1;
        triangles[i].p2 = world->triangles[i].p2;
        triangles[i].p3 = world->triangles[i].p3;
    }
    compareCamera.location.x = camera->location.x;
    compareCamera.location.y = camera->location.y;
    compareCamera.location.z = camera->location.z;
    qsort(triangles, world->numTriangles, sizeof(triangle_t), compareTriangles);
    
    // Go through all triangles
    vector_t p1Delta, p2Delta, p3Delta;
    point_t p1, p2, p3;
    uint8_t leftSel, rightSel;
    point_t left, right, center;
    for (i = 0; i < world->numTriangles; i++) {
        // Calculate the difference between point location and camera
        p1Delta.x = triangles[i].p1.x - camera->location.x;
        p1Delta.y = triangles[i].p1.y - camera->location.y;
        p1Delta.z = triangles[i].p1.z - camera->location.z;
        p2Delta.x = triangles[i].p2.x - camera->location.x;
        p2Delta.y = triangles[i].p2.y - camera->location.y;
        p2Delta.z = triangles[i].p2.z - camera->location.z;
        p3Delta.x = triangles[i].p3.x - camera->location.x;
        p3Delta.y = triangles[i].p3.y - camera->location.y;
        p3Delta.z = triangles[i].p3.z - camera->location.z;
        
        // Make sure at least one point is in front of the camera
        if ((dotProduct(p1Delta, cameraDirection) <= 0) &&
                (dotProduct(p2Delta, cameraDirection) <= 0) &&
                (dotProduct(p3Delta, cameraDirection) <= 0)) {
            continue;
        }
        
        // Calculate the screen coordinates
        p1 = pointToScreen(p1Delta,
                cameraHorizontalAngle, cameraVerticalAngle,
                anglePerPixelHorizontal, anglePerPixelVertical,
                halfWidth, halfHeight);
        p2 = pointToScreen(p2Delta,
                cameraHorizontalAngle, cameraVerticalAngle,
                anglePerPixelHorizontal, anglePerPixelVertical,
                halfWidth, halfHeight);
        p3 = pointToScreen(p3Delta,
                cameraHorizontalAngle, cameraVerticalAngle,
                anglePerPixelHorizontal, anglePerPixelVertical,
                halfWidth, halfHeight);
        
        // Determine the left point of the triangle
        left = p1;
        leftSel = 1;
        if (left.x > p2.x) {
            left = p2;
            leftSel = 2;
        }
        if (left.x > p3.x) {
            left = p3;
            leftSel = 3;
        }
        
        // Determine the right point of the triangle
        right = p3;
        rightSel = 3;
        if (((right.x < p2.x) || (leftSel == 3)) && (leftSel != 2)) {
            right = p2;
            rightSel = 2;
        }
        if ((right.x < p1.x) && (leftSel != 1)) {
            right = p1;
            rightSel = 1;
        }
        
        // Determine the center point of the triangle
        if ((leftSel + rightSel) == 3) {
            center = p3;
        } else if ((leftSel + rightSel) == 4) {
            center = p2;
        } else if ((leftSel + rightSel) == 5) {
            center = p1;
        }
        
        // Determine the number of triangles to paint
        if ((left.x == center.x) && (center.x == right.x)) {
            // One vertical line
            if ((center.x < 0) || (center.x >= frame->width)) {
                // Skip rendering if this will not actually be displayed
                continue;
            }
            
            rounding_t max = p1.y;
            if (max < p2.y) {
                max = p2.y;
            }
            if (max < p3.y) {
                max = p3.y;
            }
            rounding_t min = p1.y;
            if (min > p2.y) {
                min = p2.y;
            }
            if (min > p3.y) {
                min = p3.y;
            }
            
            rounding_t y;
            for (y = max; y > min; y--) {
                paintPixelf(frame, center.x, y, triangles[i].color);
            }
        } else if ((left.x == center.x) || (center.x == right.x)) {
            // Two points are in line vertically
            point_t top, bottom, side;
            uint8_t leftDirection;
            if (left.x == center.x) {
                if (left.y > center.y) {
                    top = left;
                    bottom = center;
                } else {
                    top = center;
                    bottom = left;
                }
                side = right;
                leftDirection = 0;
            } else {
                if (right.y > center.y) {
                    top = right;
                    bottom = center;
                } else {
                    top = center;
                    bottom = right;
                }
                side = left;
                leftDirection = 1;
            }
            
            // Calculate the slope of the lines
            rounding_t upperSlope = (top.y - side.y) / (top.x - side.x);
            rounding_t lowerSlope = (bottom.y - side.y) / (bottom.x - side.x);
            
            rounding_t x, y;
            rounding_t topY, bottomY;
            if (leftDirection) {
                // Go through triangle horizontally
                for (x = top.x; x > side.x; x--) {
                    // Calculate the min and max y values
                    topY = (upperSlope * (x - side.x)) + side.y;
                    bottomY = (lowerSlope * (x - side.x)) + side.y;
                    
                    // Paint vertical column of triangle
                    for (y = topY; y > bottomY; y--) {
                        paintPixelf(frame, x, y, triangles[i].color);
                    }
                    
                    // Catch one more paint
                    paintPixelf(frame, x, bottomY, triangles[i].color);
                    
                    // Correct sampling to the middle of the pixel
                    if ((x - floor(x)) != 0.5) {
                        x = floor(x) + 0.5;
                    }
                }
                
                // Paint one more pixel over if the side is just over the edge
                if ((side.x - fabs(side.x)) > 0.5) {
                    paintPixelf(frame, side.x, side.y, triangles[i].color);
                }
            } else {
                // Go through triangle horizontally
                for (x = top.x; x < side.x; x++) {
                    // Calculate the min and max y values
                    topY = (upperSlope * (x - side.x)) + side.y;
                    bottomY = (lowerSlope * (x - side.x)) + side.y;
                    
                    // Paint vertical column of triangle
                    for (y = topY; y > bottomY; y--) {
                        paintPixelf(frame, x, y, triangles[i].color);
                    }
                    
                    // Catch one more paint
                    paintPixelf(frame, x, bottomY, triangles[i].color);
                    
                    // Correct sampling to the middle of the pixel
                    if ((x - floor(x)) != 0.5) {
                        x = floor(x) + 0.5;
                    }
                }
                
                // Paint one more pixel over if the side is just over the edge
                if ((side.x - floor(side.x)) < 0.5) {
                    paintPixelf(frame, side.x, side.y, triangles[i].color);
                }
            }
        } else {
            // Points are not directly in line vertically
            rounding_t slopeLeftCenter = (center.y - left.y) / (center.x - left.x);
            rounding_t slopeLeftRight = (right.y - left.y) / (right.x - left.x);
            rounding_t slopeCenterRight = (right.y - center.y) / (right.x - center.x);
            rounding_t x, y;
            rounding_t topY, bottomY;
            
            // Left to center
            for (x = left.x; x < center.x; x++) {
                // Make sure rendering is only done if the point is visible
                if ((x < 0) || (x >= frame->width)) {
                    continue;
                }
                
                // Calculate the min and max y values
                topY = (slopeLeftCenter * (x - left.x)) + left.y;
                bottomY = (slopeLeftRight * (x - left.x)) + left.y;
                if (topY < bottomY) {
                    // Flip the numbers around
                    y = topY;
                    topY = bottomY;
                    bottomY = y;
                }
                
                // Paint the vertical column of the triangle
                for (y = topY; y > bottomY; y--) {
                    paintPixelf(frame, x, y, triangles[i].color);
                }
                
                // Catch one more paint
                paintPixelf(frame, x, bottomY, triangles[i].color);
                
                // Correct sampling to the middle of the pixel
                if ((x - floor(x)) != 0.5) {
                    x = floor(x) + 0.5;
                }
            }
            
            // Center to right
            for (x = center.x; x < right.x; x++) {
                // Make sure rendering is only done if the point is visible
                if ((x < 0) || (x >= frame->width)) {
                    continue;
                }
                
                // Calculate the min and max y values
                topY = (slopeCenterRight * (x - right.x)) + right.y;
                bottomY = (slopeLeftRight * (x - right.x)) + right.y;
                if (topY < bottomY) {
                    // Flip the numbers around
                    y = topY;
                    topY = bottomY;
                    bottomY = y;
                }
                
                // Paint the vertical column of the triangle
                for (y = topY; y > bottomY; y--) {
                    paintPixelf(frame, x, y, triangles[i].color);
                }
                
                // Catch one more paint
                paintPixelf(frame, x, bottomY, triangles[i].color);
                
                // Correct sampling to the middle of the pixel
                if ((x - floor(x)) != 0.5) {
                    x = floor(x) + 0.5;
                }
            }
                
            // Paint one more pixel over if the right is just over the edge
            if ((right.x - floor(right.x)) < 0.5) {
                // Make sure rendering is only done if the point is visible
                if ((right.x >= 0) && (right.x < frame->width)) {
                    paintPixelf(frame, right.x, right.y, triangles[i].color);
                }
            }
        }
    }
}

void Render_Engine_DisplayFrame(uint8_t channel, framebuffer_t *frame) {
    // Wait for the transmit buffer to clear
    while (UART_IsTransmitting(channel));
    
    // Set the cursor to the origin so the new frame tiles across the old frame
    changeTerminalCursorLocation(channel, 0, 0);
    
    uint16_t i;
    uint8_t lastColor = 0;
    
    // Access the UART through the HAL directly to get around the buffer
    for (i = 0; i < (frame->width * frame->height); i++) {
        // Move to the next row to force where the pixels are displayed
        if ((i > 0) && ((i % frame->width) == 0)) {
            writeTerminalBlock(channel, '\r');
            writeTerminalBlock(channel, '\n');
        }
        
        // Increase speed by only changing the selected color when needed
        if (lastColor != frame->buffer[i]) {
            // Change the current color
            lastColor = frame->buffer[i];
            changeTerminalColor(channel, frame->buffer[i]);
        }
        
        // Output a color block
        writeTerminalBlock(channel, ' ');
    }
}

// Rendering helper functions
point_t pointToScreen(vector_t delta,
        rounding_t camHAngle, rounding_t camVAngle,
        rounding_t angleHPixel, rounding_t angleVPixel,
        uint8_t halfWidth, uint8_t halfHeight) {
    rounding_t angleHorizontal, angleVertical;
    point_t screen;
    
    // Horizontal position onscreen
    if ((delta.x == 0) && (delta.y == 0)) {
        angleHorizontal = 0;
    } else {
        angleHorizontal = atan2(delta.y, delta.x) - camHAngle;
    }
    if (angleHorizontal <= -M_PI) {
        angleHorizontal += 2 * M_PI;
    } else if (angleHorizontal > M_PI) {
        angleHorizontal -= 2 * M_PI;
    }
    screen.x = halfWidth - (angleHorizontal / angleHPixel);
    
    // Vertical position onscreen
    if ((delta.x == 0) && (delta.y == 0) && (delta.z == 0)) {
        angleVertical = 0;
    } else {
        angleVertical = atan2(delta.z, sqrt((delta.x * delta.x) +
                (delta.y * delta.y))) - camVAngle;
    }
    screen.y = halfHeight - (angleVertical / angleVPixel);
    
    return screen;
}

rounding_t dotProduct(vector_t a, vector_t b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

int compareTriangles(const void* a, const void* b) {
    triangle_t triA = *((triangle_t *) a);
    triangle_t triB = *((triangle_t *) b);
    vector_t triACenter = {(triA.p1.x + triA.p2.x + triA.p3.x) / 3,
            (triA.p1.y + triA.p2.y + triA.p3.y) / 3,
            (triA.p1.z + triA.p2.z + triA.p3.z) / 3};
    vector_t triBCenter = {(triB.p1.x + triB.p2.x + triB.p3.x) / 3,
            (triB.p1.y + triB.p2.y + triB.p3.y) / 3,
            (triB.p1.z + triB.p2.z + triB.p3.z) / 3};
    float distA = ((triACenter.x - compareCamera.location.x) * (triACenter.x - compareCamera.location.x)) +
            ((triACenter.y - compareCamera.location.y) * (triACenter.y - compareCamera.location.y)) +
            ((triACenter.z - compareCamera.location.z) * (triACenter.z - compareCamera.location.z));
    float distB = ((triBCenter.x - compareCamera.location.x) * (triBCenter.x - compareCamera.location.x)) +
            ((triBCenter.y - compareCamera.location.y) * (triBCenter.y - compareCamera.location.y)) +
            ((triBCenter.z - compareCamera.location.z) * (triBCenter.z - compareCamera.location.z));
    
    if (distA == distB) {
        return 0;
    } else if (distA < distB) {
        return 1;
    } else {
        return -1;
    }
}

void paintPixel(framebuffer_t* frame, uint16_t x, uint16_t y, uint8_t color) {
    if ((x < frame->width) && (y < frame->height)) {
        frame->buffer[x + (y * frame->width)] = color;
    }
}

void paintPixelf(framebuffer_t* frame, rounding_t x, rounding_t y, uint8_t color) {
    if ((x >= 0) && (y >= 0)) {
        paintPixel(frame, (uint16_t) x, (uint16_t) y, color);
    }
}

// UART helper functions
void changeTerminalCursorLocation(uint8_t channel, uint8_t x, uint8_t y) {
    writeTerminalBlock(channel, '\e');
    writeTerminalBlock(channel, '[');
    writeTerminalNumber(channel, y + 1);
    writeTerminalBlock(channel, ';');
    writeTerminalNumber(channel, x + 1);
    writeTerminalBlock(channel, 'H');
}

void writeTerminalNumber(uint8_t channel, uint8_t number) {
    uint8_t atDigits = 0;
    
    // Look for hundreds
    uint8_t hundreds = (number / 100) % 10;
    if (hundreds > 0) {
        atDigits = 1;
        writeTerminalBlock(channel, hundreds + '0');
    }
    
    // Look for tens
    uint8_t tens = (number / 10) % 10;
    if ((tens > 0) || atDigits) {
        atDigits = 1;
        writeTerminalBlock(channel, tens + '0');
    }
    
    // Look for ones
    uint8_t ones = number % 10;
    if ((ones > 0) || atDigits) {
        atDigits = 1;
        writeTerminalBlock(channel, ones + '0');
    }
}

void changeTerminalColor(uint8_t channel, uint8_t color) {
    // TODO Support wider range of colors than default terminal colors
    
    writeTerminalBlock(channel, '\e');
    writeTerminalBlock(channel, '[');
    writeTerminalNumber(channel, color);
    writeTerminalBlock(channel, 'm');
}

void writeTerminalBlock(uint8_t channel, uint8_t data) {
    while (!hal_UART_SpaceAvailable(channel));
    hal_UART_TxByte(channel, data);
}
