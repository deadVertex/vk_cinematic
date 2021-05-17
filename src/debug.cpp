internal void DrawLine(
    DebugDrawingBuffer *buffer, vec3 start, vec3 end, vec3 color)
{
    if (buffer->count + 2 <= buffer->max)
    {
        VertexPC *a = buffer->vertices + buffer->count++;
        VertexPC *b = buffer->vertices + buffer->count++;

        a->position = start;
        a->color = color;
        b->position = end;
        b->color = color;
    }
}

internal void DrawBox(
    DebugDrawingBuffer *buffer, vec3 min, vec3 max, vec3 color)
{
    DrawLine(
        buffer, Vec3(min.x, min.y, min.z), Vec3(max.x, min.y, min.z), color);
    DrawLine(
        buffer, Vec3(max.x, min.y, min.z), Vec3(max.x, min.y, max.z), color);
    DrawLine(
        buffer, Vec3(max.x, min.y, max.z), Vec3(min.x, min.y, max.z), color);
    DrawLine(
        buffer, Vec3(min.x, min.y, max.z), Vec3(min.x, min.y, min.z), color);

    DrawLine(
        buffer, Vec3(min.x, max.y, min.z), Vec3(max.x, max.y, min.z), color);
    DrawLine(
        buffer, Vec3(max.x, max.y, min.z), Vec3(max.x, max.y, max.z), color);
    DrawLine(
        buffer, Vec3(max.x, max.y, max.z), Vec3(min.x, max.y, max.z), color);
    DrawLine(
        buffer, Vec3(min.x, max.y, max.z), Vec3(min.x, max.y, min.z), color);

    DrawLine(
        buffer, Vec3(min.x, min.y, min.z), Vec3(min.x, max.y, min.z), color);
    DrawLine(
        buffer, Vec3(max.x, min.y, min.z), Vec3(max.x, max.y, min.z), color);
    DrawLine(
        buffer, Vec3(max.x, min.y, max.z), Vec3(max.x, max.y, max.z), color);
    DrawLine(
        buffer, Vec3(min.x, min.y, max.z), Vec3(min.x, max.y, max.z), color);
}

internal void DrawPoint(DebugDrawingBuffer *buffer, vec3 p, f32 size, vec3 color)
{
    DrawLine(buffer, p - Vec3(size, 0, 0), p + Vec3(size, 0, 0), color);
    DrawLine(buffer, p - Vec3(0, size, 0), p + Vec3(0, size, 0), color);
    DrawLine(buffer, p - Vec3(0, 0, size), p + Vec3(0, 0, size), color);
}
