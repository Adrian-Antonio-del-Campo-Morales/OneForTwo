/*
 * OneInTwo — Contrato entre lógica común y escritorio.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Describir capacidades observadas, estados de permiso, geometría y eventos de la sesión gráfica.
 * - Separar capacidades de leer/escribir portapapeles, ofrecer archivos, capturar entrada e inyectarla.
 * - Definir operaciones asíncronas y cancelables y resultados: disponible, pendiente de permiso, denegado o no soportado.
 * - Ofrecer un único propietario del portapapeles nativo que coordine texto y ofertas de archivos.
 * - Normalizar eventos de entrada y declarar qué hilo debe ejecutar cada operación nativa.
 * - No incluir tipos de UI, sockets ni reglas de confianza; concretar firmas tras los prototipos.
 * - Windows y Linux implementarán este contrato; el estado actual contiene solo responsabilidades.
 */

