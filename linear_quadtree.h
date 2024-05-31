// LinearQuadtree.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#ifndef LINEAR_QT_H
#define LINEAR_QT_H

#include "float.h"
#include "math.h"

struct Point {
    float x = 0.0f;
    float y = 0.0f;
    float mass = 0.0f;
};

struct Node {
    bool has_children = false;
    int point_count = 0;
    Point point;
};

//https://www.youtube.com/watch?v=zuWvrqcZwuU
inline void get_child_nodes(int parent_node, int &lowBound, int &highBound) {
    lowBound = 4 * parent_node + 1;
    highBound = lowBound + 3;
}
inline void find_bounding_box(Point* points, int count, float2 &min, float2 &max) {
    min.x = FLT_MAX;
    min.y = FLT_MAX;
    max.x = FLT_MIN;
    max.y = FLT_MIN;
    for (size_t i = 0; i < count; i++)
    {
        float cx = points[i].x;
        float cy = points[i].y;
        if (cx < min.x) {
            min.x = cx;
        }
        if (cy < min.y) {
            min.y = cy;
        }
        if (cx > max.x) {
            max.x = cx;
        }
        if (cy > max.y) {
            max.y = cy;
        }
    }
}
inline bool within_bounds(const float2& min, const float2& max, const Point& point) {
    return point.x >= min.x && point.y >= min.y && point.x < max.x && point.y < max.y;
}
inline void insert_point(
    const int &depth,
    const int &nodeIndex,
    const int &maxDepth,
    Node* quadtree, 
    const float2 &min,
    const float2 &max,
    const Point& point) {

    //new node = no point and no children
    //leaf node = has point and no children
    //non leaf node = has point and children
    //error = has no point and has children

    //if the currenet node does not carry a point and has no children
    //then insert point into this current node
    bool has_point = quadtree[nodeIndex].point_count > 0;
    bool has_children = quadtree[nodeIndex].has_children;
    
    //new node
    if(!has_children && !has_point) {
        quadtree[nodeIndex].point = point;
        quadtree[nodeIndex].point_count++;
    }
    //if this node is a leaf node or non leaf node, insert point into a child node
    else if(depth != maxDepth){

        //mark this node as having children
        quadtree[nodeIndex].has_children = true;

        //get indices of child nodes
        int lowBound;
        int highBound;
        get_child_nodes(nodeIndex, lowBound, highBound);

        //get the extents of the quadrant
        float x_offset_length = (max.x - min.x) * 0.5;
        float y_offset_length = (max.y - min.y) * 0.5;

        //order: nw, ne, sw, se
        int x_offsets[] = { 0, 1, 0, 1 };
        int y_offsets[] = { 1, 1, 0, 0 };
        int quadrant = 0;

        //check if point is within a quadrant
        for (size_t i = lowBound; i <= highBound; i++)
        {
            float2 cur_min = min;
            cur_min.x += x_offsets[quadrant] * x_offset_length;
            cur_min.y += y_offsets[quadrant] * y_offset_length;

            float2 cur_max = max;
            cur_max.x += x_offsets[quadrant] * x_offset_length;
            cur_max.y += y_offsets[quadrant] * y_offset_length;

            //if point is within this quadrant, insert point into the respective node
            if (within_bounds(cur_min, cur_max, point)) {
                insert_point(depth + 1, i, maxDepth, quadtree, cur_min, cur_max, point);
                break;
            }
            quadrant++;
        }

        //if the current node carries a point but did not have children (meaning it was a leaf node prior to his operation)
        //move the point and insert it into one of the child nodes
        if (has_point && !has_children) {
            quadrant = 0;
            Point current_point = quadtree[nodeIndex].point;

            for (size_t i = lowBound; i <= highBound; i++)
            {
                float2 cur_min = min;
                cur_min.x += x_offsets[quadrant] * x_offset_length;
                cur_min.y += y_offsets[quadrant] * y_offset_length;

                float2 cur_max = max;
                cur_max.x += x_offsets[quadrant] * x_offset_length;
                cur_max.y += y_offsets[quadrant] * y_offset_length;

                if (within_bounds(cur_min, cur_max, current_point)) {
                    insert_point(depth + 1, i, maxDepth, quadtree, cur_min, cur_max, current_point);
                    break;
                }
                quadrant++;
            }
        }

        quadtree[nodeIndex].point_count++;
        quadtree[nodeIndex].point.x += point.x;
        quadtree[nodeIndex].point.y += point.y;
        quadtree[nodeIndex].point.mass += point.mass;

    }
    //if we reached max depth, compress data into a single data point
    //avoid adding more children
    else {
        quadtree[nodeIndex].has_children = false;
        quadtree[nodeIndex].point_count++;
        quadtree[nodeIndex].point.x += point.x;
        quadtree[nodeIndex].point.y += point.y;
        quadtree[nodeIndex].point.mass += point.mass;
    }
}

inline int get_quad_tree_length(int max_depth) {
    return powf(4.0f, max_depth + 1) - 1;
}

inline Node* create_tree(int max_depth) {
    int length = get_quad_tree_length(max_depth);
    auto quad_tree = new Node[length];
    memset(quad_tree, 0, sizeof(Node) * length);
    return quad_tree;
}

#endif LINEAR_QT_H
