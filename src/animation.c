// Basic Animation Helpers

void AnimateColor(f32 *Current, f32 *Target, f32 dt, f32 Speed) {
    for(int i=0; i<4; ++i) Current[i] = Lerp(Current[i], Target[i], dt * Speed);
}

// Global Animation State Moved to win32_main.c for centralization
