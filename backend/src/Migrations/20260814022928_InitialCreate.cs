using System;
using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace Namorix.Weave.Migrations
{
    /// <inheritdoc />
    public partial class InitialCreate : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "Networks",
                columns: table => new
                {
                    Id = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    Protocol = table.Column<string>(type: "TEXT", nullable: false),
                    Name = table.Column<string>(type: "TEXT", nullable: true),
                    Host = table.Column<string>(type: "TEXT", nullable: true),
                    Status = table.Column<string>(type: "TEXT", nullable: false),
                    Eui64 = table.Column<string>(type: "TEXT", nullable: true),
                    PublicKey = table.Column<string>(type: "TEXT", nullable: true),
                    FirstSeenAt = table.Column<DateTime>(type: "TEXT", nullable: true),
                    AcceptedAt = table.Column<DateTime>(type: "TEXT", nullable: true),
                    RejectedAt = table.Column<DateTime>(type: "TEXT", nullable: true),
                    CreatedAt = table.Column<DateTime>(type: "TEXT", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Networks", x => x.Id);
                });

            migrationBuilder.CreateTable(
                name: "BrThreadDataset",
                columns: table => new
                {
                    NetworkId = table.Column<int>(type: "INTEGER", nullable: false),
                    PanId = table.Column<ushort>(type: "INTEGER", nullable: false),
                    ExtendedPanId = table.Column<byte[]>(type: "BLOB", nullable: false),
                    Channel = table.Column<byte>(type: "INTEGER", nullable: false),
                    ChannelMask = table.Column<uint>(type: "INTEGER", nullable: false),
                    NetworkName = table.Column<string>(type: "TEXT", nullable: true),
                    MeshLocalPrefix = table.Column<byte[]>(type: "BLOB", nullable: false),
                    NetworkKeyEncrypted = table.Column<string>(type: "TEXT", nullable: true),
                    Pskc = table.Column<byte[]>(type: "BLOB", nullable: false),
                    SecurityPolicy = table.Column<byte[]>(type: "BLOB", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_BrThreadDataset", x => x.NetworkId);
                    table.ForeignKey(
                        name: "FK_BrThreadDataset_Networks_NetworkId",
                        column: x => x.NetworkId,
                        principalTable: "Networks",
                        principalColumn: "Id",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateIndex(
                name: "IX_Networks_Eui64",
                table: "Networks",
                column: "Eui64",
                unique: true,
                filter: "\"Eui64\" IS NOT NULL");
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "BrThreadDataset");

            migrationBuilder.DropTable(
                name: "Networks");
        }
    }
}
