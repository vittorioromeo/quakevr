void() Honey_UpdateSecrets =
{
    WriteByte(MSG_ALL, SVC_UPDATESTAT);
    WriteByte(MSG_ALL, 11); // STAT_SECRETS
    WriteLong(MSG_ALL, total_secrets);
}
