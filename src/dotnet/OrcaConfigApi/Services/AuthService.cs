using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Text;
using Microsoft.EntityFrameworkCore;
using Microsoft.IdentityModel.Tokens;
using OrcaConfigApi.Configuration;
using OrcaConfigApi.Data;
using OrcaConfigApi.DTOs;
using OrcaConfigApi.Models;

namespace OrcaConfigApi.Services;

public interface IAuthService
{
    Task<LoginResponse?> LoginAsync(LoginRequest request);
    Task<LoginResponse?> RefreshAsync(string refreshToken);
    Task<UserDto?> GetUserAsync(string userId);
}

public class AuthService : IAuthService
{
    private readonly OrcaDbContext _db;
    private readonly JwtSettings _jwtSettings;
    private readonly Dictionary<string, (string UserId, DateTime Expiry)> _refreshTokens = new();

    public AuthService(OrcaDbContext db, JwtSettings jwtSettings)
    {
        _db = db;
        _jwtSettings = jwtSettings;
    }

    public async Task<LoginResponse?> LoginAsync(LoginRequest request)
    {
        var user = await _db.Users.FirstOrDefaultAsync(u => u.Username == request.Username);
        if (user == null || !BCrypt.Net.BCrypt.Verify(request.Password, user.PasswordHash))
            return null;

        return GenerateTokens(user);
    }

    public async Task<LoginResponse?> RefreshAsync(string refreshToken)
    {
        if (!_refreshTokens.TryGetValue(refreshToken, out var tokenInfo))
            return null;

        if (tokenInfo.Expiry < DateTime.UtcNow)
        {
            _refreshTokens.Remove(refreshToken);
            return null;
        }

        var user = await _db.Users.FindAsync(Guid.Parse(tokenInfo.UserId));
        if (user == null) return null;

        _refreshTokens.Remove(refreshToken);
        return GenerateTokens(user);
    }

    public async Task<UserDto?> GetUserAsync(string userId)
    {
        var user = await _db.Users.FindAsync(Guid.Parse(userId));
        if (user == null) return null;
        return new UserDto(user.Id, user.Username, user.Email);
    }

    private LoginResponse GenerateTokens(User user)
    {
        var expiresAt = DateTime.UtcNow.AddMinutes(_jwtSettings.ExpiryMinutes);
        var refreshToken = Guid.NewGuid().ToString();
        _refreshTokens[refreshToken] = (user.Id.ToString(), DateTime.UtcNow.AddDays(7));

        var claims = new[]
        {
            new Claim(ClaimTypes.NameIdentifier, user.Id.ToString()),
            new Claim(ClaimTypes.Name, user.Username)
        };

        var key = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(_jwtSettings.Secret));
        var creds = new SigningCredentials(key, SecurityAlgorithms.HmacSha256);
        var token = new JwtSecurityToken(
            issuer: _jwtSettings.Issuer,
            audience: _jwtSettings.Audience,
            claims: claims,
            expires: expiresAt,
            signingCredentials: creds
        );

        return new LoginResponse(
            new JwtSecurityTokenHandler().WriteToken(token),
            refreshToken,
            expiresAt
        );
    }
}
