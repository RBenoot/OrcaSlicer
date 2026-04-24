namespace OrcaConfigApi.DTOs;

public record LoginRequest(string Username, string Password);
public record LoginResponse(string Token, string RefreshToken, DateTime ExpiresAt);
public record RefreshRequest(string RefreshToken);
public record UserDto(Guid Id, string Username, string? Email);
