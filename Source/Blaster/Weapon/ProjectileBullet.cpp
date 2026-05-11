#include "ProjectileBullet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/BoxComponent.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"

void AProjectileBullet::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectileBullet::OnHit);
	}
}

void AProjectileBullet::OnHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	// 伤害只允许服务端执行
	if (HasAuthority())
	{
		ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
		if (OwnerCharacter)
		{
			AController* OwnerController = OwnerCharacter->Controller;
			if (OwnerController)
			{
				UGameplayStatics::ApplyDamage(
					OtherActor,
					Damage,
					OwnerController,
					this,
					UDamageType::StaticClass()
				);
			}
		}
	}

	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ImpactParticles,
			Hit.ImpactPoint,
			Hit.ImpactNormal.Rotation()
		);
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}

	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (TracerComponent)
	{
		TracerComponent->Deactivate();
		TracerComponent->SetVisibility(false);
	}

	GetWorldTimerManager().SetTimer(
		DestroyTimer,
		this,
		&AProjectileBullet::DestroyTimerFinished,
		DestroyTime
	);
}

void AProjectileBullet::DestroyTimerFinished()
{
	Destroy();
}

void AProjectileBullet::Destroyed()
{

}