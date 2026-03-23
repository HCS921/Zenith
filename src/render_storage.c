#include "renderer.h"
#include "font.h"
#include "filesystem.h"

extern renderer_state GlobalRenderer;
extern font_atlas Font;

// Forward Decl
int CompareStorageDesc(const void *A, const void *B);

// --- Linked List Merge Sort for Storage Nodes ---
internal storage_node *
MergeStorageNodes(storage_node *A, storage_node *B)
{
    storage_node Dummy;
    storage_node *Tail = &Dummy;
    Dummy.NextSibling = 0;
    
    while(A && B)
    {
        if(A->Size >= B->Size)
        {
            Tail->NextSibling = A;
            A = A->NextSibling;
        }
        else
        {
            Tail->NextSibling = B;
            B = B->NextSibling;
        }
        Tail = Tail->NextSibling;
    }
    
    if(A) Tail->NextSibling = A;
    else Tail->NextSibling = B;
    
    return Dummy.NextSibling;
}

internal void
SplitStorageList(storage_node *Source, storage_node **Front, storage_node **Back)
{
    storage_node *Fast;
    storage_node *Slow;
    
    if(!Source || !Source->NextSibling)
    {
        *Front = Source;
        *Back = 0;
        return;
    }
    
    Slow = Source;
    Fast = Source->NextSibling;
    
    while(Fast)
    {
        Fast = Fast->NextSibling;
        if(Fast)
        {
            Slow = Slow->NextSibling;
            Fast = Fast->NextSibling;
        }
    }
    
    *Front = Source;
    *Back = Slow->NextSibling;
    Slow->NextSibling = 0;
}

internal void
SortStorageList(storage_node **HeadRef)
{
    storage_node *Head = *HeadRef;
    storage_node *A;
    storage_node *B;
    
    if(!Head || !Head->NextSibling) return;
    
    SplitStorageList(Head, &A, &B);
    
    SortStorageList(&A);
    SortStorageList(&B);
    
    *HeadRef = MergeStorageNodes(A, B);
}

// Global flag to ensure we only sort once per expansion if needed, 
// but actually we want to sort whenever we render? 
// Better: Sort "lazily" or "just-in-time" if we detect modification. 
// For now, sorting every frame is too expensive. 
// We should sort when we Populate.
// But `AnalyzeStorage` doesn't sort.
// We'll sort in Update/Expand.
// HOWEVER, for `RenderStorageNode`, we can just ensure they are sorted 
// right after we fetch them. 

// Redundant externs removed - now in app_interface.h

internal void
RenderStorageNode(storage_node *Node, f32 X, f32 *CurrentY, f32 Y_Min, f32 Y_Max, f32 W, f32 RowH, u64 MaxSizeBytes, int MouseX, int MouseY, b32 Clicked, int Indent)
{

    if(*CurrentY + RowH < Y_Min || *CurrentY > Y_Max)
    {
         *CurrentY += RowH;
         if(Node->IsExpanded && Node->FirstChild)
         {
             storage_node *Child = Node->FirstChild;
             while(Child)
             {
                 RenderStorageNode(Child, X, CurrentY, Y_Min, Y_Max, W, RowH, Node->Size, MouseX, MouseY, Clicked, Indent + 1);
                 Child = Child->NextSibling;
             }
         }
         return;
    }
    
    f32 ColNameX = X + (20 * GlobalScale) + (Indent * 20 * GlobalScale);
    f32 ColTypeX = X + W - (380 * GlobalScale);
    f32 ColSizeX = X + W - (220 * GlobalScale);
    f32 ColPctX  = X + W - (80 * GlobalScale);
    
    f32 MutedColor[] = {0.5f, 0.5f, 0.5f, 1.0f};

    if(*CurrentY + RowH >= Y_Min)
    {
        b32 IsHover = (MouseX >= X && MouseX <= X+W && MouseY >= *CurrentY && MouseY < *CurrentY+RowH);
        
        // Row Highlight
        if(IsHover) RenderQuad(&GlobalRenderer, X, *CurrentY, W, RowH, (f32[]){1,1,1,0.05f});
        
        // Chevron / Expand Check
        if(Node->IsDirectory)
        {
             f32 ChevX = ColNameX - 15*GlobalScale;
             b32 ChevHover = (MouseX >= ChevX && MouseX <= ChevX + 15*GlobalScale && MouseY >= *CurrentY && MouseY < *CurrentY + RowH);
             
             if(ChevHover && Clicked) {
                 Node->IsExpanded = !Node->IsExpanded;
                 
                 if(Node->IsExpanded && !Node->ChildrenPopulated)
                 {
                      extern memory_arena GlobalStorageArena;
                      extern storage_node *AnalyzeStorage(memory_arena *Arena, wchar_t *Path, int Depth);
                      // This will "Append" to current arena
                      storage_node *Result = AnalyzeStorage(&GlobalStorageArena, Node->FullPath, Indent + 1);
                      if(Result)
                      {
                          Node->FirstChild = Result->FirstChild;
                          Node->ChildCount = Result->ChildCount;
                          Node->ChildrenPopulated = true;
                          
                          // Sort immediately after population
                          SortStorageList(&Node->FirstChild);
                      }
                 }
                 else if(Node->IsExpanded)
                 {
                     // Ensure sorted if already populated
                     SortStorageList(&Node->FirstChild);
                 }
             }
             
             DrawTextStr(&GlobalRenderer, &Font, ChevX, *CurrentY + 8*GlobalScale, Node->IsExpanded ? "v" : ">", (f32[]){1,1,1,0.8f});
        }

        if(IsHover && Clicked && MouseX > ColNameX) 
        {
             // Select
        }

        // Name Alignment & Icon
        icon_type IType = DetermineIconType(Node->Name, Node->IsDirectory);
        Render8BitIcon(IType, ColNameX, *CurrentY + 8*GlobalScale, GlobalScale);
        
        char NameAnsi[128];
        for(int k=0; k<127; ++k) { NameAnsi[k] = (char)Node->Name[k]; if(!Node->Name[k]) break; }
        NameAnsi[127]=0;
        DrawTextStr(&GlobalRenderer, &Font, ColNameX + 28*GlobalScale, *CurrentY + 8*GlobalScale, NameAnsi, (f32[]){0.9f, 0.9f, 0.9f, 1});

        // Type
        DrawTextStr(&GlobalRenderer, &Font, ColTypeX, *CurrentY + 8*GlobalScale, Node->IsDirectory ? "Folder" : "File", MutedColor);

        if(Node->IsDirectory && (Node->Size == 0 || Node->Size == (u64)-1 || Node->Size == (u64)-2))
        {
             extern u64 GetCachedSize(wchar_t *Path);
             u64 Cached = GetCachedSize(Node->FullPath);
             if(Cached != (u64)-1) 
             {
                 Node->Size = Cached;
             }
        }
        
        // Size
        char SizeStr[64];
        if(Node->Size == (u64)-1 || Node->Size == (u64)-2)
        {
            sprintf_s(SizeStr, sizeof(SizeStr), "...");
        }
        else
        {
            double SizeBytes = (double)Node->Size;
            if(SizeBytes >= 1024.0*1024.0*1024.0) sprintf_s(SizeStr, sizeof(SizeStr), "%.2f GB", SizeBytes/(1024.0*1024.0*1024.0));
            else if(SizeBytes >= 1024.0*1024.0) sprintf_s(SizeStr, sizeof(SizeStr), "%.1f MB", SizeBytes/(1024.0*1024.0));
            else if(SizeBytes >= 1024.0) sprintf_s(SizeStr, sizeof(SizeStr), "%.1f KB", SizeBytes/1024.0);
            else sprintf_s(SizeStr, sizeof(SizeStr), "%.0f B", SizeBytes);
        }
        
        DrawTextStr(&GlobalRenderer, &Font, ColSizeX, *CurrentY + 8*GlobalScale, SizeStr, (f32[]){0.8f, 0.8f, 0.8f, 1});

        // Usage %
        f32 Pct = (double)Node->Size / (double)MaxSizeBytes;
        char PctDisp[16];
        sprintf_s(PctDisp, sizeof(PctDisp), "%.1f %%", Pct*100.0f);
        DrawTextStr(&GlobalRenderer, &Font, ColPctX, *CurrentY + 8*GlobalScale, PctDisp, (f32[]){0.85f, 0.85f, 0.85f, 0.9f});
        
        // Minimalist Bar
        f32 BarHeight = 2.0f * GlobalScale;
        f32 BarMaxW = (ColPctX - ColNameX);
        RenderQuad(&GlobalRenderer, ColNameX, *CurrentY + RowH - BarHeight, BarMaxW * Pct, BarHeight, (f32[]){0.8f, 0.8f, 0.8f, 0.6f});
    }
    *CurrentY += RowH;

    if(Node->IsExpanded && Node->FirstChild)
    {
         // Iterate Linked List - NO STACK ARRAY!
         storage_node *Child = Node->FirstChild;
         while(Child)
         {
             RenderStorageNode(Child, X, CurrentY, Y_Min, Y_Max, W, RowH, Node->Size, MouseX, MouseY, Clicked, Indent + 1);
             Child = Child->NextSibling;
         }
    }
}

internal void
RenderStorageVisualization(f32 X, f32 Y, f32 W, f32 H, storage_node *Tree)
{
    if(!Tree || !Tree->FirstChild) return;
    // NOTE: This modifies the tree structure order. That's actually fine/good.
    SortStorageList(&Tree->FirstChild);

    f32 Margin = 20 * GlobalScale;
    f32 ItemH = 60 * GlobalScale;
    f32 CurrentY = Y + Margin;
    u64 MaxSize = Tree->Size;
    
    storage_node *Node = Tree->FirstChild;
    int i = 0;
    while(Node)
    {
        if(CurrentY + ItemH > Y + H) break;
        
        f32 Pct = (MaxSize > 0) ? (f32)((double)Node->Size / (double)MaxSize) : 0;

        // Label
        char Name[128];
        for(int k=0; k<127; ++k) { Name[k] = (char)Node->Name[k]; if(!Node->Name[k]) break; }
        Name[127]=0;
        
        char SizeStr[64];
        double S = (double)Node->Size;
        if(S >= 1024.0*1024.0*1024.0) sprintf_s(SizeStr, 64, "%.1f GB", S/(1024.0*1024.0*1024.0));
        else if(S >= 1024.0*1024.0) sprintf_s(SizeStr, 64, "%.1f MB", S/(1024.0*1024.0));
        else sprintf_s(SizeStr, 64, "%.0f KB", S/1024.0);

        char Label[256];
        sprintf_s(Label, sizeof(Label), "%s (%s)", Name, SizeStr);
        DrawTextStr(&GlobalRenderer, &Font, X + Margin, CurrentY, Label, (f32[]){0.9f, 0.9f, 0.9f, 1});

        // Bar
        f32 BarW = (W - Margin*2) * Pct;
        if(BarW < 2.0f) BarW = 2.0f;
        
        f32 BarCol[] = {0.7f, 0.7f, 0.7f, 0.8f};
        if(i == 0) { BarCol[0]=0.95f; BarCol[1]=0.95f; BarCol[2]=0.95f; } // Strongest Silver
        else if(i == 1) { BarCol[0]=0.85f; BarCol[1]=0.85f; BarCol[2]=0.85f; }
        
        RenderRoundedQuad(&GlobalRenderer, X + Margin, CurrentY + 25*GlobalScale, BarW, 20*GlobalScale, 4*GlobalScale, BarCol);
        
        CurrentY += ItemH;
        Node = Node->NextSibling;
        i++;
    }
}

void
RenderStorageView(f32 X, f32 Y, f32 W, f32 H, storage_node *Tree, f32 ScrollY, int MouseX, int MouseY, b32 Clicked)
{
    
    if(!Tree) {
        DrawTextStr(&GlobalRenderer, &Font, X+20, Y+40, "No Analysis Data.", (f32[]){1,1,1,0.5f});
        return;
    }

    extern b32 GlobalStorageVisualizeMode;
    if(GlobalStorageVisualizeMode)
    {
        RenderStorageVisualization(X, Y, W, H, Tree);
        return;
    }

    f32 RowH = 34.0f * GlobalScale;
    f32 CurrentY = Y - ScrollY;
    
    
    // Header
    f32 BgActive[] = {0.12f, 0.12f, 0.12f, 1.0f};
    f32 MutedColor[] = {0.5f, 0.5f, 0.5f, 1.0f};
    RenderQuad(&GlobalRenderer, X, Y - 30*GlobalScale, W, 30*GlobalScale, BgActive);
    DrawTextStr(&GlobalRenderer, &Font, X + 20*GlobalScale, Y - 24*GlobalScale, "NAME", MutedColor);
    DrawTextStr(&GlobalRenderer, &Font, X + W - 380*GlobalScale, Y - 24*GlobalScale, "TYPE", MutedColor);
    DrawTextStr(&GlobalRenderer, &Font, X + W - 220*GlobalScale, Y - 24*GlobalScale, "SIZE", MutedColor);
    DrawTextStr(&GlobalRenderer, &Font, X + W - 80*GlobalScale, Y - 24*GlobalScale, "USAGE", MutedColor);

    RenderStorageNode(Tree, X, &CurrentY, Y, Y+H, W, RowH, Tree->Size, MouseX, MouseY, Clicked, 0);
}
