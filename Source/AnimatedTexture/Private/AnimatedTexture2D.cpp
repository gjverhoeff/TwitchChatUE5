// AnimatedTexture2D.cpp
// Copyright 2019 Neil Fang. All Rights Reserved.
// Animated Texture from GIF/WebP file
// Created by Neil Fang
// GitHub: https://github.com/neil3d/UAnimatedTexture5

#include "AnimatedTexture2D.h"
#include "AnimatedTextureResource.h"
#include "GIFDecoder.h"
#include "WebpDecoder.h"

float UAnimatedTexture2D::GetSurfaceWidth() const
{
    if (Decoder) return Decoder->GetWidth();
    return 1.0f;
}

float UAnimatedTexture2D::GetSurfaceHeight() const
{
    if (Decoder) return Decoder->GetHeight();
    return 1.0f;
}

FTextureResource* UAnimatedTexture2D::CreateResource()
{
    if (FileType == EAnimatedTextureType::None
        || FileBlob.Num() <= 0)
        return nullptr;

    // Create decoder
    switch (FileType)
    {
    case EAnimatedTextureType::Gif:
        Decoder = MakeShared<FGIFDecoder, ESPMode::ThreadSafe>();
        break;
    case EAnimatedTextureType::Webp:
        Decoder = MakeShared<FWebpDecoder, ESPMode::ThreadSafe>();
        break;
    }

    check(Decoder);
    if (Decoder->LoadFromMemory(FileBlob.GetData(), FileBlob.Num()))
    {
        AnimationLength = Decoder->GetDuration(DefaultFrameDelay * 1000) / 1000.0f;
        SupportsTransparency = Decoder->SupportsTransparency();
    }
    else
    {
        Decoder.Reset();
        return nullptr;
    }

    // Create RHI resource object
    return new FAnimatedTextureResource(this);
}

void UAnimatedTexture2D::Tick(float DeltaTime)
{
    // Only advance if we have a decoder and playback is enabled
    if (!bPlaying || !Decoder.IsValid())
    {
        return;
    }

    // Accumulate time, scaled by any PlayRate you've set
    FrameTime += DeltaTime * PlayRate;

    // If we haven't yet reached the delay for the current frame, bail out
    if (FrameTime < FrameDelay)
    {
        return;
    }

    // Time to advance to the next frame
    FrameTime = 0;

    // RenderFrameToTexture() decodes the next GIF/WebP frame,
    // pushes it into the RHI, and returns the *next* frame's delay in seconds.
    FrameDelay = RenderFrameToTexture();
}

#if WITH_EDITOR
void UAnimatedTexture2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    bool RequiresNotifyMaterials = false;
    bool ResetAnimState = false;

    FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
    if (PropertyThatChanged)
    {
        const FName PropertyName = PropertyThatChanged->GetFName();
        static const FName SupportsTransparencyName = GET_MEMBER_NAME_CHECKED(UAnimatedTexture2D, SupportsTransparency);

        if (PropertyName == SupportsTransparencyName)
        {
            RequiresNotifyMaterials = true;
            ResetAnimState = true;
        }
    }

    if (ResetAnimState)
    {
        FrameDelay = RenderFrameToTexture();
        FrameTime = 0;
    }

    if (RequiresNotifyMaterials)
        NotifyMaterials();
}
#endif // WITH_EDITOR

void UAnimatedTexture2D::ImportFile(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize)
{
    FileType = InFileType;
    FileBlob = TArray<uint8>(InBuffer, InBufferSize);
}

float UAnimatedTexture2D::RenderFrameToTexture()
{
    // Decode a new frame to memory buffer
    int nFrameDelay = Decoder->NextFrame(DefaultFrameDelay * 1000, bLooping);

    // Copy frame to RHI texture
    struct FRenderCommandData
    {
        FTextureResource* RHIResource;
        const uint8* FrameBuffer;
    };

    typedef TSharedPtr<FRenderCommandData, ESPMode::ThreadSafe> FCommandDataPtr;
    FCommandDataPtr CommandData = MakeShared<FRenderCommandData, ESPMode::ThreadSafe>();
    CommandData->RHIResource = GetResource();
    // <-- Use reinterpret_cast here instead of static_cast
    CommandData->FrameBuffer = reinterpret_cast<const uint8*>(Decoder->GetFrameBuffer());

    // Enqueue render command
    ENQUEUE_RENDER_COMMAND(AnimTexture2D_RenderFrame)(
        [CommandData](FRHICommandListImmediate& RHICmdList)
        {
            if (!CommandData->RHIResource || !CommandData->RHIResource->TextureRHI)
                return;

            // Unified RHI texture reference
            FTextureRHIRef TextureRHI = CommandData->RHIResource->TextureRHI;
            if (!TextureRHI)
                return;

            uint32 TexWidth = TextureRHI->GetSizeX();
            uint32 TexHeight = TextureRHI->GetSizeY();
            uint32 SrcPitch = TexWidth * sizeof(FColor);

            FUpdateTextureRegion2D Region;
            Region.SrcX = Region.SrcY = Region.DestX = Region.DestY = 0;
            Region.Width = TexWidth;
            Region.Height = TexHeight;

            RHIUpdateTexture2D(TextureRHI, 0, Region, SrcPitch, CommandData->FrameBuffer);
        });

    return nFrameDelay / 1000.0f;
}

float UAnimatedTexture2D::GetAnimationLength() const
{
    return AnimationLength;
}

void UAnimatedTexture2D::Play()
{
    bPlaying = true;
}

void UAnimatedTexture2D::PlayFromStart()
{
    FrameTime = 0;
    FrameDelay = 0;
    bPlaying = true;
    if (Decoder) Decoder->Reset();
}

void UAnimatedTexture2D::Stop()
{
    bPlaying = false;
}
